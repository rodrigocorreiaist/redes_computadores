#!/usr/bin/env python3

import argparse
import dataclasses
import filecmp
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import textwrap
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Sequence, Tuple


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SCRIPTS_DIR = REPO_ROOT / "SCRIPTS_25-26"
DEFAULT_EVENT_DATA_DIR = REPO_ROOT / "Event_Data"
DEFAULT_REPORTS_DIR = REPO_ROOT / "test_reports"


@dataclass(frozen=True)
class ScriptRunResult:
    script_name: str
    script_path: Path
    ok: bool
    return_code: int
    duration_s: float
    output_path: Path
    translated_path: Path
    notes: List[str]


@dataclass
class RunConfig:
    host: str
    port: int
    port_is_default: bool
    scripts_dir: Path
    reports_dir: Path
    event_data_dir: Path
    build: bool
    reset_db: bool
    start_server: bool
    verbose_server: bool
    user_timeout_s: float
    server_ready_timeout_s: float
    jobs: int
    use_repo_db: bool
    preseed_download_suite: bool


class RunnerError(RuntimeError):
    pass


def _relpath(path: Path, base: Path) -> str:
    try:
        return str(path.relative_to(base))
    except Exception:
        return str(path)


def _run_cmd(args: Sequence[str], cwd: Optional[Path] = None, timeout: Optional[float] = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        list(args),
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
        check=False,
    )


def build_project() -> None:
    res = _run_cmd(["make", "clean"], cwd=REPO_ROOT)
    if res.returncode != 0:
        raise RunnerError(f"make clean falhou\n{res.stdout}")

    res = _run_cmd(["make"], cwd=REPO_ROOT)
    if res.returncode != 0:
        raise RunnerError(f"make falhou\n{res.stdout}")


def reset_db_dirs() -> None:
    raise RunnerError("reset_db_dirs foi desativado por segurança. Usa a BD isolada (default) ou apaga manualmente USERS/ e EVENTS/.")


def _tail_bytes(path: Path, max_bytes: int = 4096) -> str:
    try:
        with open(path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            f.seek(max(0, size - max_bytes), os.SEEK_SET)
            data = f.read()
        return data.decode("utf-8", errors="replace")
    except FileNotFoundError:
        return ""


def _pick_free_tcp_port(host: str) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, 0))
        return int(s.getsockname()[1])


def _wait_for_tcp(host: str, port: int, timeout_s: float, server: Optional["ServerProcess"] = None) -> None:
    deadline = time.time() + timeout_s
    last_err: Optional[Exception] = None
    while time.time() < deadline:
        if server is not None and server.proc.poll() is not None:
            log_tail = _tail_bytes(server.log_path)
            extra = ("\n" + log_tail.strip()) if log_tail.strip() else ""
            raise RunnerError(
                f"Servidor terminou durante o arranque (exit={server.proc.returncode})."
                f" Log: {server.log_path}{extra}"
            )
        try:
            with socket.create_connection((host, port), timeout=0.3):
                return
        except Exception as e:
            last_err = e
            time.sleep(0.1)
    raise RunnerError(f"Servidor não ficou pronto em {timeout_s:.1f}s (TCP {host}:{port}): {last_err}")


@dataclass
class ServerProcess:
    proc: subprocess.Popen
    log_path: Path
    log_f: Optional[object] = None

    def stop(self, timeout_s: float = 2.0) -> None:
        if self.proc.poll() is not None:
            if self.log_f is not None:
                try:
                    self.log_f.close()
                except Exception:
                    pass
            return

        try:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                return

            try:
                self.proc.wait(timeout=timeout_s)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(self.proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    return
                self.proc.wait(timeout=timeout_s)
        finally:
            if self.log_f is not None:
                try:
                    self.log_f.close()
                except Exception:
                    pass


def start_server(port: int, verbose: bool, reports_dir: Path) -> ServerProcess:
    server_bin = REPO_ROOT / "event_server"
    if not server_bin.exists():
        raise RunnerError("Executável ./event_server não existe. Corre com --build ou faz make.")

    reports_dir.mkdir(parents=True, exist_ok=True)
    log_path = reports_dir / "server.log"

    server_args = [str(server_bin), "-p", str(port)]
    if verbose:
        server_args.append("-v")

    stdbuf = shutil.which("stdbuf")
    args = [stdbuf, "-oL", "-eL", *server_args] if stdbuf else server_args

    log_f = open(log_path, "wb")
    proc = subprocess.Popen(
        args,
        cwd=str(reports_dir),
        stdout=log_f,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )

    return ServerProcess(proc=proc, log_path=log_path, log_f=log_f)


class ScriptTranslator:
    def __init__(self, event_data_dir: Path, downloads_dir: Path):
        self._event_data_dir = event_data_dir
        self._downloads_dir = downloads_dir
        self._last_login_uid: Optional[str] = None
        self._last_login_pass: Optional[str] = None
        self._last_show_eid: Optional[str] = None
        self._compare_actions: List[Tuple[str, str]] = []

    def translate(self, raw_lines: Iterable[str]) -> Tuple[List[str], List[str]]:
        out: List[str] = []
        notes: List[str] = []

        for raw in raw_lines:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("%"):
                continue

            translated, note = self._translate_line(line)
            if translated is not None:
                out.append(translated)
            if note:
                notes.append(note)

        if not out or out[-1].strip().lower() != "exit":
            out.append("exit")
            notes.append("append: exit")

        return out, notes

    def comparisons(self) -> List[Tuple[str, str]]:
        return list(self._compare_actions)

    def _resolve_file(self, fname: str) -> str:
        p = Path(fname)
        if p.is_absolute() and p.exists():
            return str(p)

        p1 = (REPO_ROOT / p)
        if p1.exists():
            return str(p1)

        p2 = (self._event_data_dir / p.name)
        if p2.exists():
            return str(p2)

        return fname

    def _translate_line(self, line: str) -> Tuple[Optional[str], Optional[str]]:
        parts = line.split()
        if not parts:
            return None, None

        cmd = parts[0]
        cmd_l = cmd.lower()

        if cmd_l == "login" and len(parts) >= 3:
            self._last_login_uid = parts[1]
            self._last_login_pass = parts[2]
            return line, None

        if cmd_l == "show" and len(parts) == 2:
            self._last_show_eid = parts[1]
            return line, None

        if cmd == "RCOMP" and len(parts) == 2:
            if not self._last_show_eid:
                return None, "skip: RCOMP sem show anterior"
            original = self._resolve_file(parts[1])
            downloaded = str((self._downloads_dir / f"event_{self._last_show_eid}_{Path(parts[1]).name}").resolve())
            self._compare_actions.append((original, downloaded))
            return None, f"runner: compare {Path(original).name} (EID {self._last_show_eid})"

        if cmd == "changePass":
            return "changepass " + " ".join(parts[1:]), "map: changePass -> changepass"

        if cmd_l == "create" and len(parts) == 6:
            name = parts[1]
            fname = self._resolve_file(parts[2])
            date = parts[3]
            time_s = parts[4]
            cap = parts[5]
            return f"create {name} {date} {time_s} {cap} {fname}", "map: create args reorder"

        if cmd_l == "unregister" and len(parts) == 1 and self._last_login_pass:
            return f"unregister {self._last_login_pass}", "map: unregister + inferred password"

        return line, None


def load_script(path: Path) -> List[str]:
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except FileNotFoundError:
        raise RunnerError(f"Script não encontrado: {path}")


def script_path_from_number(scripts_dir: Path, number: str) -> Path:
    m = re.fullmatch(r"\d{1,2}", number.strip())
    if not m:
        raise RunnerError(f"Número de script inválido: {number}")
    nn = int(number)
    return scripts_dir / f"script_{nn:02d}.txt"


def run_user(translated_lines: Sequence[str], host: str, port: int, timeout_s: float, cwd: Path) -> Tuple[int, str, float]:
    user_bin = REPO_ROOT / "user"
    if not user_bin.exists():
        raise RunnerError("Executável ./user não existe. Corre com --build ou faz make.")

    input_data = "\n".join(translated_lines) + "\n"
    start = time.time()
    proc = subprocess.run(
        [str(user_bin), "-n", host, "-p", str(port)],
        cwd=str(cwd),
        input=input_data,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout_s,
        check=False,
    )
    dur = time.time() - start
    return proc.returncode, proc.stdout, dur


def _dir_has_entries(path: Path) -> bool:
    if not path.exists() or not path.is_dir():
        return False
    try:
        next(path.iterdir())
        return True
    except StopIteration:
        return False


def preseed_download_suite(cfg: RunConfig, cwd: Path) -> None:
    def p(name: str) -> str:
        return str((cfg.event_data_dir / name).resolve())

    lines = [
        "login 999999 aaaaaaaa",
        f"create BlackSeaGr 22-04-2026 09:00 500 {p('blackseagrain.png')}",
        f"create EmergMedic 27-04-2026 09:30 500 {p('Emergency_Medicine.png')}",
        f"create GRC2026 17-08-2026 10:00 500 {p('GRC_2026.png')}",
        f"create IENE2026 21-09-2026 10:30 500 {p('IENE_2026.png')}",
        f"create Apollo11 16-07-1969 09:32 900 {p('Apollo11_Launch.jpg')}",
        f"create Trafalgar 21-10-1805 11:45 999 {p('Trafalgar_Battle.jpg')}",
        f"create RClesson 01-12-2026 12:00 200 {p('SMTP_commands.txt')}",
        "logout",
        "exit",
    ]

    rc, out, _ = run_user(lines, host=cfg.host, port=cfg.port, timeout_s=max(30.0, cfg.user_timeout_s), cwd=cwd)
    if rc != 0:
        raise RunnerError(f"preseed falhou (rc={rc})\n{out}")
    if "Create:" in out and ("erro" in out.lower() or "falha" in out.lower()):
        raise RunnerError(f"preseed falhou\n{out}")


def _default_reports_subdir(reports_root: Path) -> Path:
    ts = time.strftime("%Y%m%d_%H%M%S")
    return reports_root / ts


def should_fail_output(output: str) -> Optional[str]:
    if "Comando desconhecido" in output:
        return "cliente reportou comando desconhecido"
    if "Erro ao conectar" in output or "Erro ao conectar ao servidor TCP" in output:
        return "cliente não conseguiu conectar ao servidor"
    if "Erro ao criar socket" in output:
        return "cliente falhou ao criar socket"
    if "Show: evento não encontrado" in output:
        return "show falhou (evento não encontrado)"
    if "Show: erro ao receber ficheiro" in output:
        return "show falhou (erro ao receber ficheiro)"
    if "Create: falha ao criar evento" in output:
        return "create falhou"
    if "timeout" in output.lower():
        return "timeout no cliente"
    return None


def compare_files(expected_path: str, actual_path: str) -> Optional[str]:
    ep = Path(expected_path)
    ap = Path(actual_path)
    if not ep.exists():
        return f"ficheiro original não existe: {ep}"
    if not ap.exists():
        return f"ficheiro recebido não existe: {ap}"
    same = filecmp.cmp(str(ep), str(ap), shallow=False)
    if not same:
        return f"ficheiros diferentes: {ep.name} vs {ap.name}"
    return None


def _scan_server_log(log_path: Path) -> List[str]:
    text = _tail_bytes(log_path, max_bytes=20000)
    issues: List[str] = []
    needles = [
        "Bind TCP failed",
        "Bind UDP failed",
        "Address already in use",
        "ERROR",
        "ERR ",
        " failed",
    ]
    for n in needles:
        if n in text:
            issues.append(f"server.log contém '{n.strip()}'")
    return issues


def run_scripts(cfg: RunConfig, script_paths: Sequence[Path], run_dir: Path, jobs: int) -> List[ScriptRunResult]:
    results: List[ScriptRunResult] = []

    def one(sp: Path) -> ScriptRunResult:
        script_dir = run_dir / sp.stem
        return run_script_one(cfg, sp, script_dir)

    if jobs <= 1 or len(script_paths) <= 1:
        for sp in script_paths:
            results.append(one(sp))
        return results

    from concurrent.futures import ThreadPoolExecutor, as_completed

    with ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(one, sp) for sp in script_paths]
        for f in as_completed(futs):
            results.append(f.result())
    results.sort(key=lambda r: r.script_name)
    return results


def run_script_one(cfg: RunConfig, script_path: Path, reports_dir: Path) -> ScriptRunResult:
    raw_lines = load_script(script_path)
    translator = ScriptTranslator(cfg.event_data_dir, reports_dir)
    translated_lines, notes = translator.translate(raw_lines)
    compares = translator.comparisons()

    reports_dir.mkdir(parents=True, exist_ok=True)

    translated_path = reports_dir / f"{script_path.stem}.translated.txt"
    translated_path.write_text("\n".join(translated_lines) + "\n", encoding="utf-8")

    output_path = reports_dir / f"{script_path.stem}.out.txt"

    rc, out, dur = run_user(
        translated_lines=translated_lines,
        host=cfg.host,
        port=cfg.port,
        timeout_s=cfg.user_timeout_s,
        cwd=reports_dir,
    )

    output_path.write_text(out, encoding="utf-8", errors="replace")

    fail_reason = None
    if rc != 0:
        fail_reason = f"return code {rc}"
    else:
        fail_reason = should_fail_output(out)

    compare_failures: List[str] = []
    if fail_reason is None and compares:
        for (expected, actual) in compares:
            err = compare_files(expected, actual)
            if err:
                compare_failures.append(err)

        if compare_failures:
            fail_reason = "comparação de ficheiros falhou"

    ok = fail_reason is None
    if fail_reason:
        notes = notes + [f"fail: {fail_reason}"]
        for err in compare_failures:
            notes.append(f"compare: {err}")

    return ScriptRunResult(
        script_name=script_path.stem,
        script_path=script_path,
        ok=ok,
        return_code=rc,
        duration_s=dur,
        output_path=output_path,
        translated_path=translated_path,
        notes=notes,
    )


def parse_args(argv: Sequence[str]) -> RunConfig:
    p = argparse.ArgumentParser(
        prog="run_tests.py",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=textwrap.dedent(
            """
            Runner local para scripts do projecto.

            - Lê `SCRIPTS_25-26/script_XX.txt`
            - Ignora linhas a começar por '%'
            - Traduções automáticas para compatibilidade com este cliente:
              - changePass -> changepass
              - create NAME FILE DATE TIME CAP -> create NAME DATE TIME CAP FILE
              - unregister -> unregister <password> (inferida do último login)

            Outputs guardados em `test_reports/<timestamp>/`.
            """
        ),
    )

    p.add_argument("--host", default="127.0.0.1", help="ES host (default: 127.0.0.1)")
    p.add_argument("--port", type=int, default=None, help="ES port (default: 58092)")
    p.add_argument("--scripts-dir", type=Path, default=DEFAULT_SCRIPTS_DIR, help="Pasta com scripts")
    p.add_argument("--event-data-dir", type=Path, default=DEFAULT_EVENT_DATA_DIR, help="Pasta com ficheiros para create")
    p.add_argument("--reports-dir", type=Path, default=DEFAULT_REPORTS_DIR, help="Pasta onde guardar outputs")

    sel = p.add_mutually_exclusive_group(required=True)
    sel.add_argument(
        "--suite",
        choices=["all", "basic", "concurrency-upload", "concurrency-download"],
        help="Suite de testes: all | basic | concurrency-upload | concurrency-download",
    )
    sel.add_argument("--scripts", nargs="+", help="Números de script (ex: 1 2 3 10)")
    sel.add_argument("--script-files", nargs="+", type=Path, help="Paths explícitos para scripts")

    p.add_argument("--no-build", action="store_true", help="Não correr make")
    p.add_argument("--reset-db", action="store_true", help="Reset apenas na BD isolada (default); não apaga a BD do repositório")
    p.add_argument("--no-server", action="store_true", help="Não arrancar ./event_server (assume já a correr)")
    p.add_argument("--server-verbose", action="store_true", help="Arrancar servidor com -v")
    p.add_argument("--user-timeout", type=float, default=20.0, help="Timeout por execução do user (s)")
    p.add_argument("--server-ready-timeout", type=float, default=2.0, help="Timeout de readiness do servidor (s)")
    p.add_argument("--jobs", type=int, default=1, help="Número de scripts em paralelo")
    p.add_argument("--use-repo-db", action="store_true", help="Usar USERS/ e EVENTS/ do repositório (não recomendado)")
    p.add_argument(
        "--preseed-download-suite",
        action="store_true",
        help="Pré-criar eventos 001–007 (para scripts 26–29) na BD isolada do run",
    )

    a = p.parse_args(list(argv))

    scripts_dir = a.scripts_dir
    if not scripts_dir.is_absolute():
        scripts_dir = (REPO_ROOT / scripts_dir).resolve()

    event_data_dir = a.event_data_dir
    if not event_data_dir.is_absolute():
        event_data_dir = (REPO_ROOT / event_data_dir).resolve()

    reports_dir = a.reports_dir
    if not reports_dir.is_absolute():
        reports_dir = (REPO_ROOT / reports_dir).resolve()

    port_is_default = a.port is None
    port = 58092 if a.port is None else int(a.port)

    cfg = RunConfig(
        host=a.host,
        port=port,
        port_is_default=port_is_default,
        scripts_dir=scripts_dir,
        reports_dir=reports_dir,
        event_data_dir=event_data_dir,
        build=not a.no_build,
        reset_db=bool(a.reset_db),
        start_server=not a.no_server,
        verbose_server=bool(a.server_verbose),
        user_timeout_s=float(a.user_timeout),
        server_ready_timeout_s=float(a.server_ready_timeout),
        jobs=max(1, int(a.jobs)),
        use_repo_db=bool(a.use_repo_db),
        preseed_download_suite=bool(a.preseed_download_suite),
    )

    if a.suite:
        cfg._suite = a.suite  # type: ignore[attr-defined]
    else:
        script_paths: List[Path] = []
        if a.scripts:
            for n in a.scripts:
                script_paths.append(script_path_from_number(cfg.scripts_dir, n))
        else:
            for pth in a.script_files:
                pp = pth if pth.is_absolute() else (REPO_ROOT / pth)
                script_paths.append(pp.resolve())
        cfg._script_paths = script_paths  # type: ignore[attr-defined]
    return cfg


def _scripts_from_numbers(cfg: RunConfig, nums: Sequence[int]) -> List[Path]:
    return [script_path_from_number(cfg.scripts_dir, str(n)) for n in nums]


def main(argv: Sequence[str]) -> int:
    try:
        cfg = parse_args(argv)
        suite: Optional[str] = getattr(cfg, "_suite", None)  # type: ignore[attr-defined]
        script_paths: List[Path] = getattr(cfg, "_script_paths", [])  # type: ignore[attr-defined]

        if suite is not None and cfg.preseed_download_suite:
            raise RunnerError("--preseed-download-suite não é suportado com --suite")

        if cfg.build:
            build_project()

        run_dir = _default_reports_subdir(cfg.reports_dir)
        run_dir.mkdir(parents=True, exist_ok=True)

        def run_phase(phase_name: str, phase_scripts: Sequence[Path], jobs: int, reset_db: bool) -> Tuple[List[ScriptRunResult], List[str]]:
            phase_dir = run_dir / phase_name
            phase_dir.mkdir(parents=True, exist_ok=True)
            work_dir = phase_dir if cfg.use_repo_db else (phase_dir / "workdir")
            work_dir.mkdir(parents=True, exist_ok=True)

            if not cfg.use_repo_db and reset_db:
                for d in (work_dir / "USERS", work_dir / "EVENTS"):
                    if d.exists():
                        shutil.rmtree(d)

            server: Optional[ServerProcess] = None
            orig_port = cfg.port
            try:
                if cfg.start_server:
                    if cfg.port_is_default:
                        cfg.port = _pick_free_tcp_port(cfg.host)
                    try:
                        server = start_server(cfg.port, cfg.verbose_server, work_dir)
                        _wait_for_tcp(cfg.host, cfg.port, cfg.server_ready_timeout_s, server=server)
                    except RunnerError as e:
                        if cfg.port_is_default:
                            log_tail = _tail_bytes(work_dir / "server.log")
                            if "Address already in use" in str(e) or "Address already in use" in log_tail:
                                if server:
                                    server.stop()
                                cfg.port = _pick_free_tcp_port(cfg.host)
                                server = start_server(cfg.port, cfg.verbose_server, work_dir)
                                _wait_for_tcp(cfg.host, cfg.port, cfg.server_ready_timeout_s, server=server)
                            else:
                                raise
                        else:
                            raise

                if cfg.preseed_download_suite:
                    if cfg.use_repo_db:
                        raise RunnerError("--preseed-download-suite não é permitido com --use-repo-db")
                    events_dir = work_dir / "EVENTS"
                    if reset_db or not _dir_has_entries(events_dir):
                        preseed_download_suite(cfg, cwd=phase_dir)

                results = run_scripts(cfg, phase_scripts, phase_dir, jobs=jobs)

                issues: List[str] = []
                if cfg.start_server:
                    issues.extend(_scan_server_log(work_dir / "server.log"))
                return results, issues
            finally:
                cfg.port = orig_port
                if server:
                    server.stop()

        all_results: List[Tuple[str, ScriptRunResult]] = []
        all_issues: List[Tuple[str, str]] = []

        if suite is None:
            phase_results, phase_issues = run_phase("run", script_paths, jobs=cfg.jobs, reset_db=cfg.reset_db)
            for r in phase_results:
                all_results.append(("run", r))
            for i in phase_issues:
                all_issues.append(("run", i))
        else:
            def run_group(group_name: str, phases: Sequence[Tuple[str, Sequence[Path], int]]) -> None:
                group_dir = run_dir / group_name
                group_dir.mkdir(parents=True, exist_ok=True)
                work_dir = group_dir if cfg.use_repo_db else (group_dir / "workdir")
                work_dir.mkdir(parents=True, exist_ok=True)

                if cfg.use_repo_db:
                    raise RunnerError("--suite não é suportado com --use-repo-db")

                for d in (work_dir / "USERS", work_dir / "EVENTS"):
                    if d.exists():
                        shutil.rmtree(d)

                server: Optional[ServerProcess] = None
                orig_port = cfg.port
                try:
                    if cfg.start_server:
                        if cfg.port_is_default:
                            cfg.port = _pick_free_tcp_port(cfg.host)
                        try:
                            server = start_server(cfg.port, cfg.verbose_server, work_dir)
                            _wait_for_tcp(cfg.host, cfg.port, cfg.server_ready_timeout_s, server=server)
                        except RunnerError as e:
                            if cfg.port_is_default:
                                log_tail = _tail_bytes(work_dir / "server.log")
                                if "Address already in use" in str(e) or "Address already in use" in log_tail:
                                    if server:
                                        server.stop()
                                    cfg.port = _pick_free_tcp_port(cfg.host)
                                    server = start_server(cfg.port, cfg.verbose_server, work_dir)
                                    _wait_for_tcp(cfg.host, cfg.port, cfg.server_ready_timeout_s, server=server)
                                else:
                                    raise
                            else:
                                raise

                    for phase_name, phase_scripts, jobs in phases:
                        phase_dir = group_dir / phase_name
                        phase_dir.mkdir(parents=True, exist_ok=True)
                        phase_results = run_scripts(cfg, phase_scripts, phase_dir, jobs=jobs)
                        for r in phase_results:
                            all_results.append((f"{group_name}/{phase_name}", r))

                    if cfg.start_server:
                        for issue in _scan_server_log(work_dir / "server.log"):
                            all_issues.append((group_name, issue))
                finally:
                    cfg.port = orig_port
                    if server:
                        server.stop()

            if suite in ("basic", "all"):
                basic_scripts = _scripts_from_numbers(cfg, list(range(1, 13)))
                run_group("basic", [("scripts", basic_scripts, 1)])

            if suite in ("concurrency-upload", "all"):
                up_scripts = _scripts_from_numbers(cfg, [21, 22, 23, 24])
                run_group("concurrency_upload", [("scripts", up_scripts, 4)])

            if suite in ("concurrency-download", "all"):
                seed = _scripts_from_numbers(cfg, [25])
                down_scripts = _scripts_from_numbers(cfg, [26, 27, 28, 29])
                run_group(
                    "concurrency_download",
                    [
                        ("seed", seed, 1),
                        ("downloads", down_scripts, 4),
                    ],
                )

        ok_n = sum(1 for _, r in all_results if r.ok)
        total = len(all_results)

        print(f"Run: {ok_n}/{total} scripts OK")
        print(f"Outputs: {run_dir}")
        if all_issues:
            print("Server log checks:")
            for phase, issue in all_issues:
                print(f"- {phase}: {issue}")

        for phase, r in all_results:
            status = "OK" if r.ok else "FAIL"
            out_rel = _relpath(r.output_path, run_dir)
            print(f"- {phase}/{r.script_name}: {status} ({r.duration_s:.2f}s) out={out_rel}")
            for note in r.notes:
                print(f"  - {note}")

        return 0 if ok_n == total else 2

    except RunnerError as e:
        print(f"Erro: {e}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as e:
        print(f"Timeout: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
