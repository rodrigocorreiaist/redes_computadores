# Event Reservation Protocol - TCP/UDP Implementation

## Descrição
Implementação completa da plataforma de reserva de eventos usando sockets TCP e UDP, com protocolo cliente-servidor totalmente especificado. Suporta criação de eventos, reservas, gestão de utilizadores e transferência de ficheiros binários.

## Estrutura do Projeto
```
├── src/
│   ├── event_server.c    - Servidor principal (TCP+UDP, select loop)
│   ├── protocol_tcp.c    - Handlers TCP (CRE, CLS, LST, SED, RID, CPS)
│   ├── protocol_udp.c    - Handlers UDP (LIN, LOU, UNR, LME, LMR)
│   ├── user.c            - Cliente com interface textual
│   └── utils.c           - Validações e I/O robusto TCP
├── include/              - Headers (.h)
├── EVENTS/               - Armazenamento de ficheiros de eventos (criado automaticamente)
├── Makefile
└── README.md
```

## Compilação
```bash
make clean
make
```

## Execução

### Servidor
```bash
# Porta padrão (58092)
./event_server

# Porta específica com modo verbose
./event_server -p 58000 -v
```

### Cliente
```bash
# Servidor local, porta padrão
./user

# Servidor remoto (exemplo: servidor dos docentes)
./user -n 193.136.138.142 -p 58000
./user -n tejo.tecnico.ulisboa.pt -p 58000
```

## Protocolo de Comunicação

### TCP - Gestão de Eventos e Reservas

Todas as operações TCP usam transferências robustas com loops até completar o byte count.

#### 1. Create Event (CRE)
**Request:** `CRE UID pass name date time capacity filename filesize\n<binary>`
- UID: 6 dígitos
- pass: 8 caracteres alfanuméricos
- name: até 10 caracteres alfanuméricos
- date: dd-mm-yyyy
- time: hh:mm
- capacity: 10-999
- filename: nome do ficheiro
- filesize: tamanho em bytes
- Seguido de dados binários do ficheiro

**Response:** 
- `RCE OK EID\n` - Sucesso, retorna Event ID (3 dígitos)
- `RCE NOK\n` - Falha
- `RCE ERR\n` - Erro de formato
- `RCE NLG\n` - Não autenticado

#### 2. Close Event (CLS)
**Request:** `CLS UID pass EID\n`

**Response:**
- `RCL OK\n` - Evento fechado
- `RCL EID\n` - Evento não existe
- `RCL USR\n` - Não é dono do evento
- `RCL FUL\n` - Evento esgotado
- `RCL ERR\n` - Erro de formato

#### 3. List Events (LST)
**Request:** `LST\n`

**Response:**
- `RLS OK N EID1 name1 date1 time1 cap1 res1 EID2 name2 ...\n`
- `RLS NOK\n` - Sem eventos

#### 4. Show Event Details (SED)
**Request:** `SED EID\n`

**Response:**
- `RSE OK name date time capacity reserved filename filesize\n<binary>`
- `RSE NOK\n` - Evento não encontrado

#### 5. Reserve Seats (RID)
**Request:** `RID UID pass EID seats\n`

**Response:**
- `RRI ACC\n` - Reserva aceite
- `RRI CLS\n` - Evento fechado
- `RRI FUL\n` - Sem lugares ou esgotado
- `RRI NOK\n` - Evento não existe
- `RRI ERR\n` - Erro de formato

#### 6. Change Password (CPS)
**Request:** `CPS UID oldpass newpass\n`

**Response:**
- `RCP OK\n` - Password alterada
- `RCP NOK\n` - Password antiga incorreta
- `RCP NLG\n` - Não autenticado
- `RCP NID\n` - Utilizador não existe

### UDP - Autenticação e Consultas

#### 1. Login (LIN)
**Request:** `LIN UID pass\n`

**Response:**
- `RLI OK\n` - Login com sucesso
- `RLI REG\n` - Novo utilizador registado
- `RLI NOK\n` - Password incorreta
- `RLI ERR\n` - Erro de formato

#### 2. Logout (LOU)
**Request:** `LOU UID pass\n`

**Response:**
- `RLO OK\n` - Logout com sucesso
- `RLO WRP\n` - Password incorreta
- `RLO UNR\n` - Utilizador não registado
- `RLO NOK\n` - Não estava autenticado

#### 3. Unregister (UNR)
**Request:** `UNR UID pass\n`

**Response:**
- `RUR OK\n` - Utilizador removido
- `RUR WRP\n` - Password incorreta
- `RUR UNR\n` - Utilizador não existe
- `RUR NOK\n` - Não estava autenticado

#### 4. My Events (LME)
**Request:** `LME UID pass\n`

**Response:**
- `RME OK [EID status ...]\n` - Lista de eventos do utilizador
- `RME NOK\n` - Sem eventos

#### 5. My Reservations (LMR)
**Request:** `LMR UID pass\n`

**Response:**
- `RMR OK [EID num_people ...]\n` - Lista de reservas
- `RMR NOK\n` - Sem reservas

## Comandos Cliente

### Comandos Implementados
```
login           - Login/registo (UDP)
logout          - Logout (UDP)
unregister      - Desregistar (UDP)
changepass      - Alterar password (TCP)
create          - Criar evento com ficheiro (TCP)
close           - Fechar evento (TCP)
list            - Listar todos eventos (TCP)
show            - Mostrar detalhes + download ficheiro (TCP)
reserve         - Reservar lugares (TCP)
myevents        - Eventos criados por mim (UDP)
myreservations  - Minhas reservas (UDP)
help            - Mostrar ajuda
exit            - Sair
```

## Características de Implementação

### Robustez TCP
- **send_all_tcp()**: Loop até enviar todos os bytes
- **recv_all_tcp()**: Loop até receber todos os bytes
- **recv_line_tcp()**: Lê até '\n' byte a byte
- Tratamento de EINTR em todas as operações
- Sem crashes em dados malformados

### Validação de Dados
- UID: exatamente 6 dígitos
- Password: exatamente 8 alfanuméricos
- Nome evento: até 10 alfanuméricos
- Data: formato dd-mm-yyyy
- Hora: formato hh:mm
- EID: 3 dígitos
- Capacidade: 10-999

### Gestão de Estado
- Eventos: 0=ativo, 1=fechado, 2=esgotado, 3=terminado
- Login persistente em memória
- Ficheiros armazenados em `EVENTS/EID_filename`
- Auto-incremento de Event IDs (001-999)

### Transferência de Ficheiros
- Limite: 10MB por ficheiro
- Formato binário preservado
- Cliente guarda ficheiros como `event_EID_filename`

## Teste com Servidor Docentes

```bash
# Conectar ao servidor oficial
./user -n 193.136.138.142 -p 58000

# Ou usar hostname
./user -n tejo.tecnico.ulisboa.pt -p 58000

## Runner de testes (Python)

Existe um runner local para executar os scripts em `SCRIPTS_25-26/` contra o teu `./event_server`.

```bash
./tools/run_tests.py --scripts 1 --reset-db
./tools/run_tests.py --scripts 1 2 3 4
./tools/run_tests.py --scripts 5 6 7 8 --event-data-dir Event_Data
```

Opções úteis:

```bash
./tools/run_tests.py --scripts 1 --no-build
./tools/run_tests.py --scripts 1 --no-server
./tools/run_tests.py --scripts 1 --server-verbose
./tools/run_tests.py --scripts 21 22 23 24 --jobs 4
```

Os outputs (stdin traduzido + stdout do `user` + log do servidor) ficam em `test_reports/<timestamp>/`.
```

## Formato de Dados

### Event Structure
```c
typedef struct Event {
    char EID[4];           // "001"-"999"
    char name[11];         // até 10 chars
    char date[11];         // dd-mm-yyyy
    char time[6];          // hh:mm
    int attendance_size;   // 10-999
    int seats_reserved;    // contador
    char owner_UID[7];     // dono do evento
    int status;            // 0-3
    char filename[256];    // nome ficheiro
    size_t filesize;       // tamanho
    struct Event *next;
} Event;
```

### User Structure
```c
typedef struct User {
    char UID[7];
    char password[9];
    int loggedIn;
    struct User *next;
} User;
```

### Reservation Structure
```c
typedef struct Reservation {
    char UID[7];
    char EID[4];
    int num_people;
    char reservation_date[20];
    struct Reservation *next;
} Reservation;
```

## Notas de Desenvolvimento

### Arquitectura
- Select loop para multiplexar TCP e UDP
- Sem threads, operações síncronas
- Cliente fecha conexão TCP após cada operação
- Servidor aceita múltiplos clientes sequencialmente

### Limitações Conhecidas
- Sem persistência em disco (dados em memória)
- Sem autenticação criptografada
- Limite de 999 eventos
- Ficheiros limitados a 10MB

### Melhorias Futuras
- Parsing de datas para detectar eventos passados
- Persistência em ficheiros/DB
- SSL/TLS para segurança
- Pool de threads para concorrência
- Compressão de ficheiros grandes
