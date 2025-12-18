#!/usr/bin/env bash
set -euo pipefail

# Remote test harness defaults
TEST_HOST=${TEST_HOST:-tejo.tecnico.ulisboa.pt}
TEST_PORT=${TEST_PORT:-59000}

usage() {
  cat <<EOF
Usage: $(basename "$0") [options] <server_ip> <server_port>

Runs remote RC tests against your server using the IST harness.
Sends "<server_ip> <server_port> <script_no>" to $TEST_HOST:$TEST_PORT via nc
and saves HTML reports under test_reports/remote/<timestamp>/.

Positional:
  server_ip        IP or DNS of your running server (e.g., 193.136.128.103)
  server_port      Port where your server listens (e.g., 58092)

Options:
  -n <list>        Script numbers to run, space/comma separated (e.g., "1,4,6" or "1 2 3").
                   If omitted, runs scripts 21-24 (concurrency upload tests - see notes below).
  -o <dir>         Output directory (default: test_reports/remote/<timestamp>)
  -H <host>        Remote harness host (default: ${TEST_HOST})
  -P <port>        Remote harness port (default: ${TEST_PORT})
  -q               Quiet output (print paths only)
  -h               Show this help

Execution modes:
  Scripts 1-12:     Sequential (one at a time)
  Scripts 21-24:    Sequential locally BUT coordinated in parallel by remote harness
                    IMPORTANT: For scripts 21-24, use 'tools/setup_concurrency_test.sh' first!
                    Then manually execute the 4 requests in separate terminals within 30 seconds.
                    The remote server coordinates the parallel execution.
  Other ranges:     Runs in parallel locally

Examples:
  $(basename "$0") 193.136.128.103 58092                # run scripts 21-24 in parallel
  $(basename "$0") -n 1,2,3 193.136.128.103 58092       # run scripts 1,2,3
  $(basename "$0") -n "4 5 6" -o out 193.136.128.103 58092

Environment:
  TEST_HOST, TEST_PORT can override default harness endpoint.
EOF
}

# Parse options
scripts=(21 22 23 24)
out_dir=""
quiet=0
while getopts ":n:o:H:P:qh" opt; do
  case "$opt" in
    n)
      # Split on comma or space
      IFS=',' read -r -a scripts <<<"${OPTARG// /,}"
      ;;
    o)
      out_dir="$OPTARG"
      ;;
    H)
      TEST_HOST="$OPTARG"
      ;;
    P)
      TEST_PORT="$OPTARG"
      ;;
    q)
      quiet=1
      ;;
    h)
      usage; exit 0
      ;;
    *)
      echo "Unknown option: -$OPTARG" >&2
      usage; exit 2
      ;;
  esac
done
shift $((OPTIND-1))

if [[ $# -lt 2 ]]; then
  echo "Missing required arguments: <server_ip> <server_port>" >&2
  usage; exit 2
fi

server_ip="$1"
server_port="$2"

# Check dependencies
if ! command -v nc >/dev/null 2>&1; then
  echo "Error: 'nc' (netcat) is required but not found in PATH" >&2
  exit 1
fi

# Prepare output dir
if [[ -z "$out_dir" ]]; then
  ts=$(date +%Y%m%d_%H%M%S)
  out_dir="test_reports/remote/${ts}"
fi
mkdir -p "$out_dir"

# Classify scripts: sequential (1-12, 21-24) vs parallel (others)
# NOTE: Scripts 21-24 are concurrency upload tests coordinated by remote harness
# They must be executed within 30 seconds but each is sent individually (not locally parallel)
declare -a seq_scripts=()
declare -a par_scripts=()

for s in "${scripts[@]}"; do
  if [[ $s -le 12 ]] || [[ $s -ge 21 ]]; then
    seq_scripts+=("$s")
  else
    par_scripts+=("$s")
  fi
done

rc=0

# Run sequential scripts one at a time
if [[ ${#seq_scripts[@]} -gt 0 ]]; then
  if [[ $quiet -eq 0 ]]; then
    echo "Running sequential tests..."
  fi
  for s in "${seq_scripts[@]}"; do
    s_num=$(printf "%02d" "$s" 2>/dev/null || echo "$s")
    out_file="${out_dir}/script_${s_num}.html"
    input_line="${server_ip} ${server_port} ${s}"

    if [[ $quiet -eq 0 ]]; then
      echo "  Script ${s} -> ${out_file}"
    fi

    if ! printf "%s\n" "$input_line" | nc "$TEST_HOST" "$TEST_PORT" >"$out_file"; then
      echo "Failed script ${s}" >&2
      rc=1
      continue
    fi

    if [[ $quiet -eq 0 ]]; then
      if grep -m1 -q "Command file: script_" "$out_file" 2>/dev/null; then
        grep -m1 "Command file: script_" "$out_file"
      fi
      echo "  Saved: ${out_file}"
    else
      echo "$out_file"
    fi
  done
fi

# Run parallel scripts (if any exist between 12 and 21)
if [[ ${#par_scripts[@]} -gt 0 ]]; then
  if [[ $quiet -eq 0 ]]; then
    echo "Running parallel tests..."
  fi
  
  declare -a pids=()
  declare -A script_files=()

  for s in "${par_scripts[@]}"; do
    s_num=$(printf "%02d" "$s" 2>/dev/null || echo "$s")
    out_file="${out_dir}/script_${s_num}.html"
    input_line="${server_ip} ${server_port} ${s}"
    script_files[$s]="$out_file"

    if [[ $quiet -eq 0 ]]; then
      echo "  Launching script ${s} -> ${out_file}"
    fi

    (printf "%s\n" "$input_line" | nc "$TEST_HOST" "$TEST_PORT" >"$out_file" 2>&1) &
    pids+=($!)
  done

  # Wait for all parallel jobs
  for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
      rc=1
    fi
  done

  # Report parallel results
  if [[ $quiet -eq 0 ]]; then
    echo "Parallel tests completed."
    for s in "${par_scripts[@]}"; do
      out_file="${script_files[$s]}"
      if grep -m1 -q "Command file: script_" "$out_file" 2>/dev/null; then
        echo "  Script $(printf "%02d" "$s"): $(grep -m1 'Command file: script_' "$out_file")"
      fi
      echo "  Saved: ${out_file}"
    done
  else
    for s in "${par_scripts[@]}"; do
      echo "${script_files[$s]}"
    done
  fi
fi

exit "$rc"
