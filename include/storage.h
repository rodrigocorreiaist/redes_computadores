#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

int storage_init();

int storage_register_user(const char *uid, const char *pass);
int storage_check_password(const char *uid, const char *pass);
int storage_login_user(const char *uid);
int storage_logout_user(const char *uid);
int storage_is_logged_in(const char *uid);
int storage_unregister_user(const char *uid);
int storage_user_exists(const char *uid);
int storage_change_password(const char *uid, const char *new_pass);

int storage_create_event(const char *uid, const char *name, const char *date, const char *time, 
                        int attendance, const char *fname, const void *fdata, size_t fsize, char *eid_out);
int storage_close_event(const char *uid, const char *eid);
int storage_is_event_closed(const char *eid);
int storage_get_event_owner(const char *eid, char *uid_out);

/* Returns total reserved seats in the event after the reservation on success (>0).
     Returns a negative error code on failure.
 */
int storage_reserve(const char *uid, const char *eid, int num_seats);

int storage_list_events(char *buffer, size_t max_len);
int storage_list_my_events(const char *uid, char *buffer, size_t max_len);
int storage_list_my_reservations(const char *uid, char *buffer, size_t max_len);

int storage_get_event_details(const char *eid, char *uid_out, char *name_out, char *date_out, 
                             char *time_out, int *attendance_out, int *reserved_out, 
                             char *fname_out, size_t *fsize_out);
int storage_get_event_file_data(const char *eid, const char *fname, void *buffer, size_t size);

#endif
