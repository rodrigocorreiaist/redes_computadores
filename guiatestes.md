###### REDES DE COMPUTADORES

```
3 oANO LEIC 2025/2026-2oP
```
GUIA DE TESTE DE SERVIDORES ES 14DEZ2025 21:

#### 1 Teste autom ́atico remoto de servidores ES

O presente guia cont ́em os procedimentos para execu ̧c ̃ao remota de aplica ̧c ̃oesuser na maquina ‘tejo’ para teste dos servidores ES desen-
volvidos pelos alunos.

##### 1.1 Introdu ̧c ̃ao

No ˆambito do projecto de comunica ̧c ̃ao usando a interface sockets oferece-se a possibilidade de testar as aplica ̧c ̃oes desenvolvidas pelos
alunos em comunica ̧c ̃ao com aplica ̧c ̃oes que cumpram o protocolo especificado.
Tal ́e a finalidade do servidor concorrente ES em execu ̧c ̃ao no porto 58000 da m ́aquina ‘tejo’, o qual permite testar as aplica ̧c ̃oesuser
desenvolvidas pelos alunos.
Com a finalidade inversa de testar as aplica ̧c ̃oes ES desenvolvidas pelos alunos, existe em execu ̧c ̃ao na m ́aquina ‘tejo’ um servidor TCP
concorrente (no porto 59000) que inicia localmente instˆancias da aplica ̧c ̃aouser em resposta a acessos TCP pornetcat, originados pelos
alunos. Num acesso t ́ıpico, os alunos especificam os parˆametros de execu ̧c ̃ao remota da aplica ̧c ̃aouser na m ́aquina ‘tejo’.

Assim ́e poss ́ıvel executar em simultˆaneo na m ́aquina ‘tejo’ instˆancias da aplica ̧c ̃aouser que enviam mensagens a diferentes servidores
ES alvo instalados na rede p ́ublica ou na rede do T ́ecnico. Como por exemplo nas m ́aquinassigma.

Ambas as modalidades de teste acima referidas permitem aos alunos suportar tanto o desenvolvimento das suas aplica ̧c ̃oes como a compo-
nente de autoavalia ̧c ̃ao.


##### 1.2 Instru ̧c ̃oes de activa ̧c ̃ao dos testes

As aplica ̧c ̃oesuserexecutadas remotamente na m ́aquina ‘tejo’ v ̃ao ler as sequˆencias de comandos a executar descriptspredefinidos, escol-
hidos pelos alunos para testar os seus servidores ES, mantendo separa ̧c ̃ao de dados entre si n ̃ao havendo assim qualquer interferˆencia entre
aplica ̧c ̃oesuserque possam estar em execu ̧c ̃ao simultˆanea.

A figura que se segue ilustra uma sequˆencia de teste t ́ıpica sendo a mesma executada automaticamente e sem interrup ̧c ̃ao em trˆes fases.

##### tejo (LT5)

##### Servidor

##### TCP

##### User

```
script_nn
```
## netcat

## ES

#### Te ste remoto

#### de servidor ES

##### Pedido de teste

# {

##### ES_IP

##### ES_port

##### script_nn

#### Resultado do teste

#### em formato HTML

##### [ES_IP, ES_port]

### 1

### 2

### 3 port 59000

```
Figure 1: Teste remoto de servidores ES
```
A primeira fase do teste consiste na activa ̧c ̃ao da aplica ̧c ̃aouser no ‘tejo’ - assinalada na figura com© 1. Nessa activa ̧c ̃ao s ̃ao especificados
o endere ̧co IP (ESIP) e o porto (ESport) do servidor ES a testar. S ̃ao tamb ́em especificados o n ́umero doscript(scriptnn) que cont ́em
os comandos a executar pela aplica ̧c ̃aouser, e o nome do ficheiro de relat ́orio a ser enviado para o computador que activa o teste.
No seguimento da activa ̧c ̃ao, a aplica ̧c ̃aouserexecuta oscriptescolhido em acesso ao servidor ES dos alunos - segunda fase assinalada na


figura com© 2.
A terceira fase,© 3 , consiste no envio pelo sistema de testes no tejo do relat ́orio para o computador que activou o teste. Esse relat ́orio segue
em formato HTML, para ser visualizado usando umbrowserde internet.

Para executar um teste subordinado a um dadoscript, os alunos tˆem de compor e executar a seguinte linha de comando no terminal
Linux:

```
echo “targetIP targetport script”|nc tejo.tecnico.ulisboa.pt 59000>report.html
```
O texto entre “” constitui amensagem de comandona qual, ‘targetIP’ ́e o endere ̧co IP da m ́aquina onde executam o seu servidor
AS, ‘targetport’ ́e o porto da sua aplica ̧c ̃ao ES na referida m ́aquina e ‘script’ ́e um inteiro com um ou dois d ́ıgitos que indica o n ́umero do
scriptde teste que querem correr (verscriptspublicados na p ́agina da disciplina).
A linha acima descrita n ̃ao deve ser segmentada. Isto ́e: N ̃ao deve ser executado primeiramente o comandonce depois inserida a mensagem
de comando j ́a dentro do ambientenc. Porque entre a aceita ̧c ̃ao da liga ̧c ̃ao TCP no servidor e a leitura da mensagem de comando existe
uma reduzida tolerˆancia de tempo para evitar o bloqueio de recursos no servidor.

Na mensagem com o formato apresentado acima, o servidor no tejo espera encontrar o caracter ‘\n’ logo a seguir ao n ́umero doscript, o
qual ́e inserido pelo pr ́oprio comando ‘echo’ nas instala ̧c ̃oesLinuxque serviram para elaborar este guia. Caso o comando ‘echo’ n ̃ao insira
o ‘\n’ pode ser usado por exemplo o seguinte procedimento alternativo:

```
printf “targetIP targetport script\n”|nc tejo.tecnico.ulisboa.pt 59000>report.html
```
Os espa ̧cos entre campos da mensagem de comando podem ser em qualquer n ́umero desde que o comprimento total da mensagem (in-
cluindo ‘\n’) n ̃ao exceda os 25 bytes. N ̃ao ser ̃ao aceites pedidos que definam como alvo os endere ̧cos IP p ́ublico ou privado do tejo, ou
come ̧cados por 127.


No final da execu ̧c ̃ao, o servidor envia um ficheiro HTML com o resultado da referida execu ̧c ̃ao obtido pela aplica ̧c ̃aousercuja execu ̧c ̃ao se
solicitou. Esse ficheiro pode ser convertido em formato pdf para ser entregue no contexto da auto-avalia ̧c ̃ao do projecto.

O servidor ES alvo a testar pode ser executado nosigmacujo endere ̧co IPv4 pode ser obtido executando o comandohostname -i. Note-se
que osigma pode ser endere ̧cado por v ́arios IPs. Caso queiram testar o servidor ES nos seus domic ́ılios, os alunos devem proceder `a
configura ̧c ̃ao deport forwardingnos seusroutersde acesso `a rede.

Como exemplo, considere-se o servidor ES a testar no porto 58050 dosigma com IP=193.136.128.103 para execu ̧c ̃ao doscript n ́umero

6. A linha a executar numa m ́aquina com Linux (podendo ser ela o pr ́opriosigma) ser ́a:

```
echo “193.136.128.103 58050 6”|nc tejo.tecnico.ulisboa.pt 59000>report.html
```
Ficando o relat ́orio da execu ̧c ̃ao guardado no ficheiro report.html na m ́aquina na qual se executa o comandonc.

##### 1.3 Teste de servidores ES usandoscripts de comandos

Osscriptsde comando da aplica ̧c ̃aouser no tejo invocada remotamente, est ̃ao em regra numerados de acordo com o grau de complexidade
dos testes que produzem. No entanto, os alunos podem solicitar a execu ̧c ̃ao dosscripts pela ordem que entenderem conveniente para
testarem aspectos particulares do funcionamento dos seus servidores.

Aconselha-se o estudo pormenorizado dosscriptsantes das respectivas execu ̧c ̃oes para que sejam percebidas de forma adequada as funcional-
idades que s ̃ao testadas pelos mesmos e assim se possam interpretar convenientemente os resultados dos relat ́orios. Osscriptsencontram-se
devidamente comentados no sentido de facilitar a sua interpreta ̧c ̃ao.
Apresenta-se a seguir uma descri ̧c ̃ao sum ́aria dosscripts pela ordem natural de numera ̧c ̃ao dos mesmos sugerindo-se o teste do servidor
pela mesma ordem de numera ̧c ̃ao dosscripts.


##### 1.4 Descri ̧c ̃ao dosscripts de teste

Pressup ̃oe-se a execu ̧c ̃ao sequencial dosscripts descritos a seguir, devendo a base de dados encontrar-se vazia de utilizadores e de eventos
antes da execu ̧c ̃ao doscript 01.
Pressup ̃oe-se tamb ́em que no ES a testar se desactiva temporariamente a valida ̧c ̃ao da data de ocorrˆencia dos eventos. Para que seja poss ́ıvel
criar eventos com datas anteriores `a data de realiza ̧c ̃ao dos testes.

Os primeiros trˆesscripts testam funcionalidades b ́asicas do servidor ES exclusivamente atrav ́es de comunica ̧c ̃oes UDP. Estas funcional-
idades est ̃ao relacionadas com o registo e identifica ̧c ̃ao dos utilizadores e valida ̧c ̃ao depasswordsna base de dados do ES.
O quartoscripttesta a funcionalidade de altera ̧c ̃ao de password que j ́a ́e suportada em TCP.
Osscripts 5, 6, 7 e 8 testam a transferˆencia de ficheiros com os comandoscreate(upload) eshow, (download) para trˆes utilizadores
diferentes. Oscript8 compara os ficheiros recebidos com os originais enviados com os comandoscreate.

Com osscripts numerados de 1 a 8 executados em sequˆencia testam-se assim funcionalidades b ́asicas do servidor baseadas em UDP e
TCP.

Com osscriptsnumerados de 9 a 12 apresentados a seguir testam-se funcionalidades de gest ̃ao e consistˆencia da base de dados do servidor.
Para executar esta sequˆencia de testes deve come ̧car-se por inicializar de novo a base de dados removendo todos os utilizadores j ́a registados
e todos os eventos criados.

Oscript9 preenche a base de dados criando 7 eventos de um utilizador. Dois dos eventos est ̃ao no passado devendo aparecer assinal-
ados como tal nas listagens onde figuram.

Oscript10 come ̧ca por registar quatro novos utilizadores. De seguida, cada um desses novos utilizadores reserva lugares nos primeiros
quatro eventos criados peloscript 09 , pedido sucessivamente listagens de reservas e finalmente detalhes sobre os quatro eventos em causa.
A finalizar, oscript10 faz entrar em sess ̃ao o utilizador que criou os eventos para um pedido de listagemmyevents.


Oscript 11 testa a acumula ̧c ̃ao de reservas nos eventos 001 a 004. Ficando no final do teste os eventos 003 e 004 no estado de esgo-
tados. No final doscript, o utilizador que criou os eventos fecha os eventos 003, 004 e 007 - este ́ultimo um evento para o qual n ̃ao foram
efectuadas reservas.
Oscript12 testa de novo a acumula ̧c ̃ao de reservas nos eventos 001 a 004 e 007. Ficando no final do teste os eventos 001 e 002 esgotados e
devendo as reservas nos eventos 003, 004 e 007 ser rejeitadas.

##### 1.5 Testes de concorrˆencia

Os testes propostos nesta sec ̧c ̃ao est ̃ao orientados para avaliar as capacidades de concorrˆencia do servidor. Eles baseiam-se em dois conjuntos
de quatroscriptscada.
O primeiro conjunto, constitu ́ıdo pelosscriptsnumerados de 21 a 24, idˆenticos, serve para testar a concorrˆencia do servidor emuploadcom
quatro aplica ̧c ̃oesuserem acesso simultˆaneo para cria ̧c ̃ao de 10 eventos cada um.
Oscript 25 constitui no servidor uma base de dados para testar concorrˆencia na descarga de ficheiros com o comandoshow. A concorrˆencia
ser ́a testada pelo segundo conjunto descriptsnumerados de 26 a 29.

Para testar a concorrˆencia em cria ̧c ̃ao de eventos (upload) deve garantir-se em primeiro lugar que a base de dados do ES est ́a vazia, e
que tem quatro utilizadores com os seguintes pares UID/password registados: 111111/aaaaaaaa, 222222/bbbbbbbb, 333333/cccccccc e
444444/dddddddd.
De seguida abrem-se quatro terminais e neles se editam (sem executar) os comandos:

echo “TargetIP TargetPort 21”|nc tejo.ist.utl.pt 59000>report1.html

echo “TargetIP TargetPort 22”|nc tejo.ist.utl.pt 59000>report2.html


echo “TargetIP TargetPort 23”|nc tejo.ist.utl.pt 59000>report3.html

echo “TargetIP TargetPort 24”|nc tejo.ist.utl.pt 59000>report4.html

Depois de editados os quatro comandos, o utilizador d ́a em cada terminal a ordem de execu ̧c ̃ao de cada comando, por forma a que
entre a primeira ordem e a ́ultima n ̃ao decorram mais de 30 segundos. Caso este tempo seja ultrapassado, o teste ter ́a de ser repetido.

As aplica ̧c ̃oesuseractivadas no ‘tejo’ “percebem” pela numera ̧c ̃ao dosscriptsenvolvidos que devem arrancar em simultˆaneo. Portanto, s ́o
ao quarto pedido de execu ̧c ̃ao ́e que todas arrancam.

Os resultados dos quatro testes encontram-se nos relat ́orios produzidos os quais devem ser analisados pois contˆem a informa ̧c ̃ao rele-
vante para aferir das capacidades de concorrˆencia do servidor. Estes quatro testes simultˆaneos podem servir para aferir a capacidade do
servidor para resolver colis ̃oes de acesso `a base de dados do ES em opera ̧c ̃oes de escrita.

Para testar a concorrˆencia do servidor ES em opera ̧c ̃oes de descarga de ficheiros, deve a base de dados encontrar-se limpa e com o utilizador
UID/password==111111/aaaaaaaa registado.

A execu ̧c ̃ao doscript 25 serve para povoar a base de dados com os mesmos 7 eventos doscript 09.

Ap ́os a execu ̧c ̃ao doscript 25, podem ser executados em simultˆaneo os restantes quatro numerados de 26 a 29 tal como foram execu-
tados osscripts numerados de 21 a 24,mutatis mutandis. Osscripts numerados de 26 a 29 s ̃ao parecidos entre si embora com diferentes
ordena ̧c ̃oes. Ap ́os a descarga de cada ficheiro, o mesmo ́e comparado com o original (comandos RCOMP) para aferir do bom funcionamento
das opera ̧c ̃oes deuploadedownload.


