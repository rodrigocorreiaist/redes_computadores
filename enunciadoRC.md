# Redes de Computadores

## 1º Semestre — P2 — 2025/2026
**LEIC Alameda**

### Programming using the Sockets Interface
### Event Reservation

```
Date: 12 DEC 2025
```

---

## 1. Introduction

The goal of this project is to implement an **event reservation platform**. Users can create events, cancel or close them, list ongoing events, and make reservations.

The project requires implementing:

- an **Event Reservation Server (ES)**
- a **User Application (User)**

The **ES** and multiple **User** instances operate simultaneously on different machines connected to the Internet. The **ES** runs on a machine with a known IP address and port.

The **User** application supports two roles:

1. **Event management** — create, close, and list owned events
2. **Event reservation** — list events, reserve seats, list reservations

Any registered user can perform both roles.

---

## 2. Project Specification

### 2.1 User Application (User)

Invocation:

```bash
./user [-n ESIP] [-p ESport]
```

- **ESIP** — IP address of the ES (optional; defaults to localhost)
- **ESport** — well-known ES port (optional; default: `58000 + GN`)

On first login, the user ID (**UID**, 6-digit IST number) is registered on the ES.

---

### 2.2 User Commands

#### Session Management (UDP unless stated)

- **login** `UID password`
Logs in or registers a new user.

- **changePass** `oldPassword newPassword` (TCP)

- **unregister**
Unregisters the logged-in user and logs out.

- **logout**

- **exit**
Local command. Requires logout first.

---

#### Event Management

- **create** `name event_fname event_date num_attendees` (TCP)
- `name`: up to 10 alphanumeric characters
- `event_date`: `dd-mm-yyyy hh:mm`
- `num_attendees`: 10–999

- **close** `EID` (TCP)

- **myevents** / **mye** (UDP)

---

#### Event Discovery & Reservation

- **list** (TCP)

- **show** `EID` (TCP)

- **reserve** `EID value` (TCP)

- **myreservations** / **myr** (UDP)
Returns up to 50 most recent reservations.

---

Only **one command** may be active at a time.
**All ES responses must be displayed to the user.**

---

## 3. Event Reservation Server (ES)

Invocation:

```bash
./ES [-p ESport] [-v]
```

- **-p ESport** — server port (default: `58000 + GN`)
- **-v** — verbose mode (logs UID, request type, IP, and port)

The ES runs:

- a **UDP server** for user/session management
- a **TCP server** for event management and file transfers

---

## 4. Communication Protocols

### 4.1 User–ES Protocol (UDP)

Messages end with `\n`. Fields are space-separated.

#### Login

```
LIN UID password
RLI status
```

- `OK` — login successful
- `NOK` — incorrect password
- `REG` — new user registered

---

#### Logout

```
LOU UID password
RLO status
```

Statuses: `OK`, `NOK`, `UNR`, `WRP`

---

#### Unregister

```
UNR UID password
RUR status
```

Statuses: `OK`, `NOK`, `UNR`, `WRP`

---

#### My Events

```
LME UID password
RME status [EID state]*
```

Event state:

- `0` — past
- `1` — future, open
- `2` — future, sold out
- `3` — closed

---

#### My Reservations

```
LMR UID password
RMR status [EID date value]*
```

- Up to 50 most recent reservations

---

### 4.2 User–ES Protocol (TCP)

#### Create Event

```
CRE UID password name event_date attendance_size Fname Fsize Fdata
```

Reply:

```
RCE status [EID]
```

---

#### Close Event

```
CLS UID password EID
RCL status
```

Statuses include: `OK`, `NOK`, `NLG`, `NOE`, `EOW`, `SLD`, `PST`, `CLO`

---

#### List Events

```
LST
RLS status [EID name state event_date]*
```

---

#### Show Event

```
SED EID
RSE status [UID name event_date attendance_size seats_reserved Fname Fsize Fdata]
```

---

#### Reserve Seats

```
RID UID password EID people
RRI status [n_seats]
```

Statuses: `ACC`, `REJ`, `CLS`, `SLD`, `PST`, `NLG`, `WRP`

---

#### Change Password

```
CPS UID oldPassword newPassword
RCP status
```

---

## 5. Development

### 5.1 Environment

Code must compile and run correctly in **LT4** or **LT** labs.

### 5.2 Programming Language

Implementation in **C or C++**, using:

- sockets (UDP/TCP)
- `select()` for multiplexing
- robust `read()` / `write()` handling

---

## 6. Submission

### 6.1 Contents

- User and ES source code
- Makefile
- auto-avaliação Excel file
- auxiliary files
- `readme.txt`

### 6.2 Packaging

- Single ZIP archive
- Compiles with `make`
- Naming: `proj_<group>.zip`

### 6.3 Deadline

**December 19, 2025 — 23:59**

---

## 7. Open Issues

Students are encouraged to think about protocol extensions.
