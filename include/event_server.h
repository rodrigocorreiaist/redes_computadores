#ifndef EVENT_SERVER_H
#define EVENT_SERVER_H

// Estrutura de Lista Ligada para Utilizadores
typedef struct User {
    char UID[7];
    char password[9];
    int loggedIn;
    struct User *next;
} User;

// Estrutura de Lista Ligada para Eventos
typedef struct Event {
    char EID[4]; // 3 dígitos + terminador
    char name[11];
    char date[11];  // dd-mm-yyyy
    char time[6];   // hh:mm
    int attendance_size;
    int seats_reserved;
    char owner_UID[7];
    int status; // 0: ativo, 1: fechado, 2: esgotado, 3: terminado
    char filename[256];
    size_t filesize;
    struct Event *next;
} Event;

// Estrutura de Lista Ligada para Reservas
typedef struct Reservation {
    char UID[7];
    char EID[4];
    int num_people;
    char reservation_date[20];
    struct Reservation *next;
} Reservation;

// Variáveis globais (cabeças das listas)
extern User *user_list;
extern Event *event_list;
extern Reservation *reservation_list;
extern int total_events; // Contador para gerar EIDs únicos




// Declarações de funções
int login_user(char *UID, char *password);
int logout_user(char *UID, char *password);
int register_user(char *UID, char *password);
int create_event(char *UID, char *name, char *date, char *time, int attendance_size, const char *filename, size_t filesize);
int close_event(char *UID, char *EID);
void list_events();
int reserve_seats(char *UID, char *EID, int num_people);
void list_reservations(char *UID);
char* get_event_filepath(const char *EID);
int is_user_logged_in(char *UID);
void list_my_events(char *UID);
int show_event(char *EID);
int change_password(char *UID, char *old_password, char *new_password);
int unregister_user(char *UID, char *password);


#endif // EVENT_SERVER_H)