# Event Reservation Platform — Project Specification

## 1. Introduction

This project delivers an event‑reservation platform composed of two components:  
an **Event‑reservation Server (ES)** and a **User Application (User)**. Multiple User
instances and the ES may run simultaneously across networked machines. The ES runs on
a machine with a known IP address and port.

The User application supports:

- **Event management**: create, close, and list personal events.  
- **Event reservation**: list existing events, reserve seats, and view personal reservations.  

Every registered user may perform both roles. The User interface relies on keyboard
commands, each resulting in communication with the ES over TCP or UDP depending on the
operation.

### Supported User Commands

- **login** – authenticate or register a user.  
- **create** – create a new event with metadata and an associated file.  
- **close** – close an event created by the user.  
- **myevents** – list events created by the logged‑in user.  
- **list** – list all active events.  
- **show** – view details and file description for a selected event.  
- **reserve** – reserve seats for a selected event.  
- **myreservations** – list the user’s most recent 50 reservations.  
- **changePass** – change the account password.  
- **unregister** – remove the user account from the platform.  
- **logout** – sign out the current user.  
- **exit** – terminate the User application.

The implementation relies on application‑layer protocols over TCP and UDP using the
socket interface.

## 2. Project Specification

### 2.1 User Application (User)

Invocation:
```
./user [-n ESIP] [-p ESport]
```

- **ESIP** – IP of the Event‑reservation Server (optional; defaults to localhost).  
- **ESport** – TCP/UDP port of the ES (optional; defaults to `58000 + GN`).  

Once running, the User application can manage events, make reservations, and perform
account actions.

### User Commands (Detailed)

#### Login
```
login UID password
```
Sent over UDP. ES validates credentials or registers a new user if UID does not exist.

#### Change Password
```
changePass oldPassword newPassword
```
Sent via TCP. ES updates the stored password if valid.

#### Unregister
```
unregister
```
Sent via UDP. Removes user account and logs the user out.

#### Logout
```
logout
```
Sent via UDP. Logs out the user.

#### Exit
```
exit
```
Local termination; requires prior logout.

#### Create Event
```
create name event_fname event_date num_attendees
```
Sent via TCP. Creates a new event with name, description file, event date/time, and seat
capacity.

#### Close Event
```
close EID
```
Sent via TCP. Closes an event owned by the logged‑in user.

#### List Personal Events
```
myevents
```
Sent via UDP. Lists the user’s events and their status.

#### List All Events
```
list
```
Sent via TCP. Returns the full list of events.

#### Show Event
```
show EID
```
Sent via TCP. Retrieves full event details and its associated file.

#### Reserve Seats
```
reserve EID value
```
Sent via TCP. Requests seat reservations.

#### My Reservations
```
myreservations
```
Sent via UDP. Lists up to the 50 most recent reservations.

Only one command can be active at a time. All ES responses must be displayed clearly to
the user.

### 2.2 Event‑reservation Server (ES)

Invocation:
```
./ES [-p ESport] [-v]
```

- **ESport** – ES listening port (defaults to `58000 + GN`).  
- **-v** – verbose mode printing request summaries.

The ES runs both a UDP server (user/session management) and a TCP server (file transfer,
event operations).

Requests must be processed immediately when received.

## 3. Communication Protocols Specification

UID values are always 6 digits; EID values are 3 digits.

### 3.1 UDP Protocol (User ↔ ES)

#### Login
```
LIN UID password
```
Response:
```
RLI status
```
Status may be: `OK`, `NOK`, `REG`.

#### Logout
```
LOU UID password
```
Response:
```
RLO status
```
Status: `OK`, `NOK`, `UNR`, `WRP`.

#### Unregister
```
UNR UID password
```
Response:
```
RUR status
```

#### My Events
```
LME UID password
```
Response:
```
RME status [EID state]*
```
State meanings:  
1 — open;  
0 — past;  
2 — sold out;  
3 — closed.

#### My Reservations
```
LMR UID password
```
Response:
```
RMR status [EID date value]*
```
Up to 50 reservations returned.

All messages end with `
`. Any unexpected or invalid message returns `ERR`.

### 3.2 TCP Protocol (User ↔ ES)

#### Create Event
```
CRE UID password name event_date attendance_size Fname Fsize Fdata
```
Response:
```
RCE status [EID]
```

#### Close Event
```
CLS UID password EID
```
Response:
```
RCL status
```
Includes statuses like: `OK`, `NOE`, `EOW`, `SLD`, `PST`, `CLO`.

#### List Events
```
LST
```
Response:
```
RLS status [EID name state event_date]*
```

#### Show Event
```
SED EID
```
Response:
```
RSE status [UID name event_date attendance_size Seats_reserved Fname Fsize Fdata]
```

#### Reserve Seats
```
RID UID password EID people
```
Response:
```
RRI status [n_seats]
```

#### Change Password
```
CPS UID oldPassword newPassword
```
Response:
```
RCP status
```

Filenames are limited to 24 characters; maximum file size is 10 MB. All messages end
with `
`. Invalid syntax returns `ERR`.

## 4. Development

### 4.1 Environment
Your code must compile and run correctly in the lab environment (LT4 or LT5).

### 4.2 Programming Requirements

System calls you may need:

- Input: `fgets()`  
- String ops: `sscanf()`, `sprintf()`  
- UDP: `socket()`, `bind()`, `sendto()`, `recvfrom()`, `close()`  
- TCP client: `socket()`, `connect()`, `write()`, `read()`, `close()`  
- TCP server: `socket()`, `bind()`, `listen()`, `accept()`, `close()`  
- Multiplexing: `select()`

### 4.3 Notes

- Handle partial reads/writes.  
- Never crash on malformed messages.  
- Report errors cleanly without abrupt termination.

## 5. Bibliography

- Stevens, *Unix Network Programming*, Vol. 1, 2nd Ed., 1998.  
- Comer, *Computer Networks and Internets*, 2nd Ed., 1999.  
- Donahoo & Calvert, *TCP/IP Sockets in C*, 2000.  
- `man` pages  
- *Code Complete* — http://www.cc2e.com/  
- http://developerweb.net/viewforum.php?id=70

## 6. Project Submission

### 6.1 Code Requirements

Submit all source code, Makefile, and the auto‑evaluation spreadsheet.  
`make` must compile all components to the current directory.

### 6.2 Auxiliary Files

Include any additional required resources plus a `readme.txt`.

### 6.3 Submission Method

Send a single ZIP archive to the course instructor no later than **December 19, 2025,
23:59**.

Archive contents:

- Source code  
- Makefile  
- Auxiliary files  
- Auto‑evaluation file  

Archive name format:

```
proj_<group number>.zip
```
