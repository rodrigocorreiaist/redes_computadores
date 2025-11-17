#ifndef EVENT_SERVER_H
#define EVENT_SERVER_H

typedef struct {
    char UID[7];
    char password[9];
    int loggedIn;
} User;

typedef struct {
    char EID[4]; // E + 3digitos + terminador //
    char name[11];
    char event_date[20];
    int attendance_size;
    int seats_reserved;
    char owner_UID[7];
    int status;
} Event;

typedef struct {
    char UID[7];
    char EID[4];
    int num_people;
    char reservation_date[20];
} Reservation;


// Declarações de funções
int login_user(char *UID, char *password);
int logout_user(char *UID);
int register_user(char *UID, char *password);
int create_event(char *UID, char *name, char *event_date, int attendance_size);
int close_event(char *UID, char *EID);
void list_events();
int reserve_seats(char *UID, char *EID, int num_people);
void list_reservations(char *UID);
int is_user_logged_in(char *UID);
void list_my_events(char *UID);
int show_event(char *EID);
int change_password(char *UID, char *old_password, char *new_password);
int unregister_user(char *UID, char *password);


#endif // EVENT_SERVER_H)