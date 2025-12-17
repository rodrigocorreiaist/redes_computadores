# Event Booking Server (TCP/UDP)

A client/server event booking system built for a Computer Networks assignment.

- `event_server`: single process that serves **UDP + TCP on the same port** (default: `58092`)
- `user`: interactive CLI client that talks to the server
- Persistence is file-based under `USERS/` and `EVENTS/` (created automatically at runtime)

## Table of contents

- [Features](#features)
- [Project structure](#project-structure)
- [Build](#build)
- [Run](#run)
- [Client commands](#client-commands)
- [Network protocol (summary)](#network-protocol-summary)
- [Persistence layout](#persistence-layout)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [Create a new clean GitHub repo](#create-a-new-clean-github-repo)

## Features

- UDP commands for authentication & user-specific queries
- TCP commands for event lifecycle, reservations and file transfer
- Fork-per-connection TCP handling (concurrent TCP clients)
- Robust TCP I/O helpers (`send_all_tcp`, `recv_all_tcp`, `recv_line_tcp`)
- Input validation:
  - `UID`: exactly 6 digits
  - password: exactly 8 alphanumeric chars
  - event name: 1..10 alphanumeric chars
  - date: `DD-MM-YYYY`
  - time: `HH:MM`
  - `EID`: 3 digits (`001`..`999`)
- Event description file upload/download (max `10MB`)

## Project structure

```
.
├── include/
│   ├── protocol_udp.h
│   ├── protocol_tcp.h
│   ├── storage.h
│   └── utils.h
├── src/
│   ├── event_server.c
│   ├── protocol_udp.c
│   ├── protocol_tcp.c
│   ├── storage.c
│   ├── user.c
│   └── utils.c
├── SCRIPTS_25-26/              # example client scripts
├── Event_Data/
├── testes1a12.sh               # remote harness runner
├── Makefile
└── README.md
```

## Build

Requirements: GNU `make` and `gcc` on Linux.

```bash
make clean
make
```

This produces:
- `./event_server`
- `./user`

## Run

### Server

Default port is `58092`.

```bash
./event_server
```

Custom port + verbose logs:

```bash
./event_server -p 58000 -v
```

Notes:
- The server binds **both UDP and TCP** sockets to the same port.
- On first run it creates the persistence directories `USERS/` and `EVENTS/`.

### Client

Defaults: server `127.0.0.1`, port `58092`.

```bash
./user
```

Connect to a different server/port:

```bash
./user -n 193.136.128.103 -p 58000
```

You can also run a script file by redirecting stdin:

```bash
./user -n 127.0.0.1 -p 58092 < SCRIPTS_25-26/script_01.txt
```

## Client commands

Inside the `user` prompt:

- `login <UID> <password>` (UDP)
- `logout` (UDP)
- `unregister <password>` (UDP)
- `changepass <old_password> <new_password>` (TCP)
- `create <name> <file_path> <DD-MM-YYYY> <HH:MM> <capacity>` (TCP)
- `close <EID>` (TCP)
- `list` (TCP)
- `show <EID>` (TCP)
- `reserve <EID> <seats>` (TCP)
- `myevents` / `mye` (UDP)
- `myreservations` / `myr` (UDP)
- `help`
- `exit`

### `create` example

```text
login 111111 abababab
create Party Event_Data/SMTP_commands.txt 25-12-2026 18:30 100
```

The client sends the file contents over TCP (max 10MB) and the server stores it.

### `show` downloads files

`show <EID>` prints the event details and downloads the stored file to:
- `event_<EID>_<original_filename>` in the current working directory.

## Network protocol (summary)

This section is a practical summary based on the current implementation.

### UDP

All UDP requests end with `\n`.

- Login/register:
  - `LIN <UID> <pass>\n`
  - replies: `RLI OK\n | RLI REG\n | RLI NOK\n | RLI ERR\n`
- Logout:
  - `LOU <UID> <pass>\n`
  - replies: `RLO OK\n | RLO WRP\n | RLO UNR\n | RLO NOK\n | RLO ERR\n`
- Unregister:
  - `UNR <UID> <pass>\n`
  - replies: `RUR OK\n | RUR WRP\n | RUR UNR\n | RUR NOK\n | RUR ERR\n`
- List my events:
  - `LME <UID> <pass>\n`
  - replies:
    - `RME OK <EID> <state> [<EID> <state> ...]\n`
    - `RME NOK\n | RME NLG\n | RME WRP\n | RME ERR\n`
- List my reservations:
  - `LMR <UID> <pass>\n`
  - replies:
    - `RMR OK <EID> <date> <time> <seats> [ ... ]\n`
    - `RMR NOK\n | RMR NLG\n | RMR WRP\n | RMR ERR\n`

### TCP

Each TCP connection handles exactly one command, then closes.

- List events:
  - `LST\n`
  - reply:
    - `RLS OK` followed by zero or more entries: ` <EID> <name> <state> <date> <time>` and a final `\n`
    - or `RLS NOK\n` on error
- Show event details (includes file download):
  - `SED <EID>\n`
  - reply:
    - `RSE OK <uid> <name> <date> <time> <capacity> <reserved> <filename> <filesize> ` then `<filesize>` raw bytes then `\n`
    - `RSE NOK\n | RSE ERR\n`
- Reserve seats:
  - `RID <UID> <pass> <EID> <seats>\n`
  - replies: `RRI ACC\n | RRI REJ <remaining>\n | RRI CLS\n | RRI SLD\n | RRI PST\n | RRI NLG\n | RRI WRP\n | RRI NOK\n | RRI ERR\n`
- Change password:
  - `CPS <UID> <oldpass> <newpass>\n`
  - replies: `RCP OK\n | RCP NOK\n | RCP NLG\n | RCP ERR\n`
- Create event (file upload):
  - The client sends:
    - header line: `CRE <UID> <pass> <name> <date> <time> <capacity> <filename>\n`
    - size line: `<filesize>\n`
    - payload: `<filesize>` raw bytes
    - terminator: `\n`
  - replies: `RCE OK <EID>\n | RCE NOK\n | RCE NLG\n | RCE ERR\n`
- Close event:
  - `CLS <UID> <pass> <EID>\n`
  - replies: `RCL OK\n | RCL NOE\n | RCL EOW\n | RCL CLO\n | RCL NOK\n | RCL NLG\n | RCL ERR\n`

### Event state values

The code uses integer states:
- `0`: past (event date/time already passed)
- `1`: open
- `2`: sold out
- `3`: closed

## Persistence layout

The server uses the current working directory to create and manage:

- `USERS/<UID>/`:
  - `<UID>pass.txt` (password)
  - `<UID>login.txt` (exists when user is logged in)
  - `CREATED/` (marker files `<EID>.txt` for events created by the user)
  - `RESERVED/` (reservation files)
- `EVENTS/<EID>/`:
  - `START<EID>.txt` (owner, name, filename, capacity, date, time)
  - `RES<EID>.txt` (total reserved seats)
  - `END<EID>.txt` (created when closed or when event becomes past)
  - `DESCRIPTION/<filename>` (stored uploaded file)
  - `RESERVATIONS/` (reservation files)

To reset the “database”, stop the server and delete `USERS/` and `EVENTS/`.

## Testing

### Remote harness (IST)

Use `testes1a12.sh` to run the official remote harness against **your running server**.

```bash
./testes1a12.sh <server_ip> <server_port>
```

Run specific scripts:

```bash
./testes1a12.sh -n 1,2,3 <server_ip> <server_port>
```

Outputs are saved as HTML under:
- `test_reports/remote/<timestamp>/`

Environment variables:
- `TEST_HOST` (default: `tejo.tecnico.ulisboa.pt`)
- `TEST_PORT` (default: `59000`)

### Local scripts

You can run the provided scripts locally by feeding them into the client:

```bash
./event_server -v
./user < SCRIPTS_25-26/script_01.txt
```

## Troubleshooting

- `Bind ... failed`: the port is in use. Choose another port: `./event_server -p 58xxx`.
- Client says “not logged in”: most operations require `login` first.
- `Create: timeout ... (10 seconds)`: the server didn’t reply in time or the connection broke.
- Files not found on `create`: the client reads the file path you provide (local filesystem).
- Want a clean state: delete `USERS/` and `EVENTS/` (server stopped).

## Create a new clean GitHub repo

If you want a **fresh repo without the old history**, do not reuse the existing `.git/`.

### Option A (recommended): new folder + new git history

```bash
# from the parent directory
cd /home/rodrigo
cp -a redes_computadores redes_computadores_clean
cd redes_computadores_clean
rm -rf .git

# remove runtime/generated artifacts if they exist
rm -rf USERS EVENTS test_reports
rm -f src/*.o event_server user

git init
git add .
git commit -m "Initial commit"
```

Create the GitHub repo:

- Via GitHub UI: create an empty repository (no README/license; you already have them locally)
- Or via GitHub CLI (requires `gh auth login`):

```bash
gh repo create <your-username>/<new-repo-name> --public --source=. --remote=origin --push
```

### Option B: keep folder, wipe history (destructive)

Inside the current project folder:

```bash
rm -rf .git
git init
git add .
git commit -m "Initial commit"
```

Then add the new remote and push:

```bash
git remote add origin git@github.com:<your-username>/<new-repo-name>.git
git branch -M main
git push -u origin main
```
