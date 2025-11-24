# Event Reservation Project

## Descrição
Este projeto implementa uma plataforma de reserva de eventos, composta por um servidor de eventos (Event Server - ES) e uma aplicação de usuário (User).

## Estrutura do Projeto
- `src/`: Código-fonte
	- `event_server.c`: inicialização de sockets e loop principal (select)
	- `protocol_udp.c`: processamento dos comandos UDP (LIN, LOU, UNR, LME, LMR)
	- `protocol_tcp.c`: (placeholder) futuros comandos TCP (CRE, CLS, LST, SED, RID, CPS)
	- `utils.c`: validações e interpretação de respostas no cliente
	- `user.c`: aplicação cliente (interface textual)
- `include/`: cabeçalhos (`event_server.h`, `protocol_udp.h`, `protocol_tcp.h`, `utils.h`)
- `Makefile`: regras de compilação
- `README.md`: documentação do projeto

## Compilação
Para compilar o projeto, execute:
```
make clean
make
```

## Execução

Iniciar servidor (verbose opcional):
```
./event_server -v
```
Ou especificando porta:
```
./event_server -p 58000 -v
```

Iniciar cliente (mesma máquina / porta padrão):
```
./user
```

Ou apontando IP/porta:
```
./user -n 127.0.0.1 -p 58000
```

## Comandos UDP já suportados
- `login` -> LIN
- `logout` -> LOU
- `unregister` -> UNR
- `myevents` -> LME
- `myreservations` -> LMR

## Próximos passos (TCP)
Implementar gradualmente:
- `create` (CRE)
- `close` (CLS)
- `list` (LST)
- `show` (SED)
- `reserve` (RID)
- `changePass` (CPS)

## Estado do evento
Atualmente calculado de forma simplificada (aceitando reservas=1, fechado=3, esgotado=2). Falta distinguir passado (0) via parsing de data.

## Limpeza e melhorias futuras
- Separar lógica em `users.c`, `events.c`, `reservations.c`
- Parsing real de datas com `strptime`
- Persistência opcional em ficheiros
- Implementar envio e receção de ficheiro na operação `show` (SED)
