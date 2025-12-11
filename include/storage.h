#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

// Initialize storage (create USERS and EVENTS directories)
int storage_init();

// User management
int storage_register_user(const char *uid, const char *pass);
int storage_check_password(const char *uid, const char *pass);
int storage_login_user(const char *uid);
int storage_logout_user(const char *uid);
int storage_is_logged_in(const char *uid);
int storage_unregister_user(const char *uid);
int storage_user_exists(const char *uid);
int storage_change_password(const char *uid, const char *new_pass);

// Event management
// Returns 0 on success, -1 on error. Writes generated EID to eid_out (buffer size >= 4)
int storage_create_event(const char *uid, const char *name, const char *date, const char *time, 
                        int attendance, const char *fname, const void *fdata, size_t fsize, char *eid_out);
int storage_close_event(const char *uid, const char *eid);
int storage_is_event_closed(const char *eid);
int storage_get_event_owner(const char *eid, char *uid_out);

// Reservation management
int storage_reserve(const char *uid, const char *eid, int num_seats);

// Listing functions (populate buffers with protocol-formatted strings)
int storage_list_events(char *buffer, size_t max_len); // For LST
int storage_list_my_events(const char *uid, char *buffer, size_t max_len); // For LME
int storage_list_my_reservations(const char *uid, char *buffer, size_t max_len); // For LMR

// Event details
int storage_get_event_details(const char *eid, char *uid_out, char *name_out, char *date_out, 
                             char *time_out, int *attendance_out, int *reserved_out, 
                             char *fname_out, size_t *fsize_out);
int storage_get_event_file_data(const char *eid, const char *fname, void *buffer, size_t size);

#endif
