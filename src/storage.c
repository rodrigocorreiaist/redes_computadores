#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include "storage.h"
#include "utils.h" // For is_date_past

#define USERS_DIR "USERS"
#define EVENTS_DIR "EVENTS"
#define PATH_MAX_LEN 512
#define UID_LEN 6
#define EID_LEN 3
#define FNAME_MAX_LEN 255
#define RES_NAME_MAX_LEN 64

// Local fallback comparator mimicking alphasort
static int dirent_alphasort(const struct dirent **a, const struct dirent **b) {
    return strcoll((*a)->d_name, (*b)->d_name);
}

// Helper to create directory if not exists
static int ensure_dir(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) == -1) return -1;
    }
    return 0;
}

int storage_init() {
    if (ensure_dir(USERS_DIR) != 0) return -1;
    if (ensure_dir(EVENTS_DIR) != 0) return -1;
    return 0;
}

int storage_user_exists(const char *uid) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/%.*spass.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    return (access(path, F_OK) == 0);
}

int storage_register_user(const char *uid, const char *pass) {
    if (storage_user_exists(uid)) return -1; // Already exists

    char dir_path[PATH_MAX_LEN];
    snprintf(dir_path, sizeof(dir_path), "%s/%.*s", USERS_DIR, UID_LEN, uid);
    if (ensure_dir(dir_path) != 0) return -1;

    char pass_path[PATH_MAX_LEN];
    snprintf(pass_path, sizeof(pass_path), "%s/%.*s/%.*spass.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    
    FILE *fp = fopen(pass_path, "w");
    if (!fp) return -1;
    fprintf(fp, "%s", pass);
    fclose(fp);

    // Create subdirectories
    char created_path[PATH_MAX_LEN];
    snprintf(created_path, sizeof(created_path), "%s/%.*s/CREATED", USERS_DIR, UID_LEN, uid);
    ensure_dir(created_path);

    char reserved_path[PATH_MAX_LEN];
    snprintf(reserved_path, sizeof(reserved_path), "%s/%.*s/RESERVED", USERS_DIR, UID_LEN, uid);
    ensure_dir(reserved_path);

    return 0;
}

int storage_check_password(const char *uid, const char *pass) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/%.*spass.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) return 0; // User not found or error
    
    char stored_pass[32];
    if (fgets(stored_pass, sizeof(stored_pass), fp)) {
        // Remove newline if present
        stored_pass[strcspn(stored_pass, "\n")] = 0;
        fclose(fp);
        return (strcmp(stored_pass, pass) == 0);
    }
    fclose(fp);
    return 0;
}

int storage_login_user(const char *uid) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/%.*slogin.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "Logged in"); // Content doesn't matter
    fclose(fp);
    return 0;
}

int storage_logout_user(const char *uid) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/%.*slogin.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    if (unlink(path) != 0) return -1;
    return 0;
}

int storage_is_logged_in(const char *uid) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/%.*slogin.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    return (access(path, F_OK) == 0);
}

int storage_unregister_user(const char *uid) {
    char path[PATH_MAX_LEN];
    // Remove pass.txt
    snprintf(path, sizeof(path), "%s/%.*s/%.*spass.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    unlink(path);
    
    // Remove login.txt
    snprintf(path, sizeof(path), "%s/%.*s/%.*slogin.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    unlink(path);
    
    // Directories CREATED and RESERVED remain as per spec
    return 0;
}

int storage_change_password(const char *uid, const char *new_pass) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/%.*spass.txt", USERS_DIR, UID_LEN, uid, UID_LEN, uid);
    
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "%s", new_pass);
    fclose(fp);
    return 0;
}

// --- Event Helpers ---

static int get_next_eid(char *eid_out) {
    struct dirent **namelist;
    int n;
    int max_eid = 0;

    n = scandir(EVENTS_DIR, &namelist, NULL, dirent_alphasort);
    if (n < 0) return -1;

    for (int i = 0; i < n; i++) {
        if (namelist[i]->d_name[0] != '.') {
            int id = atoi(namelist[i]->d_name);
            if (id > max_eid) max_eid = id;
        }
        free(namelist[i]);
    }
    free(namelist);

    sprintf(eid_out, "%03d", max_eid + 1);
    return 0;
}

int storage_create_event(const char *uid, const char *name, const char *date, const char *time, 
                        int attendance, const char *fname, const void *fdata, size_t fsize, char *eid_out) {
    
    if (get_next_eid(eid_out) != 0) return -1;

    char event_dir[PATH_MAX_LEN];
    snprintf(event_dir, sizeof(event_dir), "%s/%.*s", EVENTS_DIR, EID_LEN, eid_out);
    if (ensure_dir(event_dir) != 0) return -1;

    // START file
    char start_path[PATH_MAX_LEN];
    snprintf(start_path, sizeof(start_path), "%s/%.*s/START%.*s.txt", EVENTS_DIR, EID_LEN, eid_out, EID_LEN, eid_out);
    FILE *fp = fopen(start_path, "w");
    if (!fp) return -1;
    fprintf(fp, "%s %s %s %d %s %s", uid, name, fname, attendance, date, time);
    fclose(fp);

    // RES file
    char res_path[PATH_MAX_LEN];
    snprintf(res_path, sizeof(res_path), "%s/%.*s/RES%.*s.txt", EVENTS_DIR, EID_LEN, eid_out, EID_LEN, eid_out);
    fp = fopen(res_path, "w");
    if (!fp) return -1;
    fprintf(fp, "0");
    fclose(fp);

    // DESCRIPTION dir and file
    char desc_dir[PATH_MAX_LEN];
    snprintf(desc_dir, sizeof(desc_dir), "%s/%.*s/DESCRIPTION", EVENTS_DIR, EID_LEN, eid_out);
    ensure_dir(desc_dir);
    
    char file_path[PATH_MAX_LEN];
    snprintf(file_path, sizeof(file_path), "%s/%.*s/DESCRIPTION/%.*s", EVENTS_DIR, EID_LEN, eid_out, FNAME_MAX_LEN, fname);
    fp = fopen(file_path, "wb");
    if (!fp) return -1;
    fwrite(fdata, 1, fsize, fp);
    fclose(fp);

    // RESERVATIONS dir
    char reservations_dir[PATH_MAX_LEN];
    snprintf(reservations_dir, sizeof(reservations_dir), "%s/%.*s/RESERVATIONS", EVENTS_DIR, EID_LEN, eid_out);
    ensure_dir(reservations_dir);

    // Add to user's CREATED dir
    char user_created_path[PATH_MAX_LEN];
    snprintf(user_created_path, sizeof(user_created_path), "%s/%.*s/CREATED/%.*s.txt", USERS_DIR, UID_LEN, uid, EID_LEN, eid_out);
    fp = fopen(user_created_path, "w");
    if (fp) fclose(fp); // Empty file

    return 0;
}

int storage_is_event_closed(const char *eid) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/END%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    return (access(path, F_OK) == 0);
}

int storage_close_event(const char *uid, const char *eid) {
    // Check ownership
    char owner[7];
    if (storage_get_event_owner(eid, owner) != 0) return -1; // Event not found
    if (strcmp(owner, uid) != 0) return -2; // Not owner

    if (storage_is_event_closed(eid)) return 0; // Already closed

    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/END%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    
    char date_str[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(date_str, sizeof(date_str), "%02d-%02d-%04d %02d:%02d:%02d", 
             t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
             t->tm_hour, t->tm_min, t->tm_sec);
            
    fprintf(fp, "%s", date_str);
    fclose(fp);
    return 0;
}

int storage_get_event_owner(const char *eid, char *uid_out) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/START%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        sscanf(buffer, "%s", uid_out);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return -1;
}

// Helper to read current reservations count
static int get_reservations_count(const char *eid) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/RES%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    int count = 0;
    fscanf(fp, "%d", &count);
    fclose(fp);
    return count;
}

static void update_reservations_count(const char *eid, int new_total) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/%.*s/RES%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%d", new_total);
        fclose(fp);
    }
}

int storage_reserve(const char *uid, const char *eid, int num_seats) {
    // Check if event exists and get details
    char start_path[PATH_MAX_LEN];
    snprintf(start_path, sizeof(start_path), "%s/%.*s/START%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    FILE *fp = fopen(start_path, "r");
    if (!fp) return -1; // Event not found

    char owner[7], name[100], fname[100], date[11], time_str[6];
    int attendance;
    fscanf(fp, "%s %s %s %d %s %s", owner, name, fname, &attendance, date, time_str);
    fclose(fp);

    // Check if closed
    if (storage_is_event_closed(eid)) return -2; // Closed

    // Check date
    if (is_date_past(date, time_str)) {
        // Close event automatically
        char end_path[PATH_MAX_LEN];
        snprintf(end_path, sizeof(end_path), "%s/%.*s/END%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
        FILE *end_fp = fopen(end_path, "w");
        if (end_fp) {
            fprintf(end_fp, "%s %s:00", date, time_str); // Use event date as end date
            fclose(end_fp);
        }
        return -2; // Closed
    }

    int current_res = get_reservations_count(eid);
    if (current_res + num_seats > attendance) return -3; // Full

    // Create reservation file
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02d%02d%02d%02d", 
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    
    char res_filename[64];
    snprintf(res_filename, sizeof(res_filename), "R-%s-%s.txt", uid, timestamp);

    char res_content[100];
    sprintf(res_content, "%s %d %02d-%02d-%04d %02d:%02d:%02d", 
            uid, num_seats,
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec);

    // Write to EVENTS/.../RESERVATIONS
    char event_res_path[PATH_MAX_LEN];
    snprintf(event_res_path, sizeof(event_res_path), "%s/%.*s/RESERVATIONS/%.*s", EVENTS_DIR, EID_LEN, eid, RES_NAME_MAX_LEN, res_filename);
    fp = fopen(event_res_path, "w");
    if (!fp) return -4;
    fprintf(fp, "%s", res_content);
    fclose(fp);

    // Write to USERS/.../RESERVED
    char user_res_path[PATH_MAX_LEN];
    snprintf(user_res_path, sizeof(user_res_path), "%s/%.*s/RESERVED/%.*s", USERS_DIR, UID_LEN, uid, RES_NAME_MAX_LEN, res_filename);
    fp = fopen(user_res_path, "w");
    if (fp) {
        fprintf(fp, "%s", res_content);
        fclose(fp);
    }

    update_reservations_count(eid, current_res + num_seats);
    return 0;
}

int storage_list_events(char *buffer, size_t max_len) {
    struct dirent **namelist;
    int n;
    
    buffer[0] = '\0';
    
    n = scandir(EVENTS_DIR, &namelist, NULL, dirent_alphasort);
    if (n < 0) return -1;

    for (int i = 0; i < n; i++) {
        if (namelist[i]->d_name[0] != '.') {
            char eid[4];
            strncpy(eid, namelist[i]->d_name, 3);
            eid[3] = '\0';

            char start_path[PATH_MAX_LEN];
            snprintf(start_path, sizeof(start_path), "%s/%.*s/START%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
            FILE *fp = fopen(start_path, "r");
            if (fp) {
                char owner[7], name[100], fname[100], date[11], time_str[6];
                int attendance;
                fscanf(fp, "%s %s %s %d %s %s", owner, name, fname, &attendance, date, time_str);
                fclose(fp);

                int closed = storage_is_event_closed(eid);
                int current_res = get_reservations_count(eid);
                int state = 1; // Open
                if (closed) state = 3; // Closed
                else if (current_res >= attendance) state = 2; // Sold out
                else if (is_date_past(date, time_str)) {
                     state = 0; 
                }

                char entry[200];
                snprintf(entry, sizeof(entry), " %s %s %d %s %s", eid, name, state, date, time_str);
                if (strlen(buffer) + strlen(entry) < max_len) {
                    strcat(buffer, entry);
                }
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    return 0;
}

int storage_list_my_events(const char *uid, char *buffer, size_t max_len) {
    char dir_path[PATH_MAX_LEN];
    snprintf(dir_path, sizeof(dir_path), "%s/%.*s/CREATED", USERS_DIR, UID_LEN, uid);
    
    struct dirent **namelist;
    int n = scandir(dir_path, &namelist, NULL, dirent_alphasort);
    if (n < 0) return -1;

    buffer[0] = '\0';

    for (int i = 0; i < n; i++) {
        if (namelist[i]->d_name[0] != '.') {
            char eid[4];
            strncpy(eid, namelist[i]->d_name, 3);
            eid[3] = '\0';

            // Get state
            char start_path[PATH_MAX_LEN];
            snprintf(start_path, sizeof(start_path), "%s/%.*s/START%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
            FILE *fp = fopen(start_path, "r");
            if (fp) {
                char owner[7], name[100], fname[100], date[11], time_str[6];
                int attendance;
                fscanf(fp, "%s %s %s %d %s %s", owner, name, fname, &attendance, date, time_str);
                fclose(fp);

                int closed = storage_is_event_closed(eid);
                int current_res = get_reservations_count(eid);
                int state = 1; // Open
                if (closed) state = 3; // Closed
                else if (current_res >= attendance) state = 2; // Sold out
                else if (is_date_past(date, time_str)) state = 0; // Past

                char entry[50];
                snprintf(entry, sizeof(entry), " %s %d", eid, state);
                if (strlen(buffer) + strlen(entry) < max_len) {
                    strcat(buffer, entry);
                }
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    return 0;
}

int storage_list_my_reservations(const char *uid, char *buffer, size_t max_len) {
    char dir_path[PATH_MAX_LEN];
    snprintf(dir_path, sizeof(dir_path), "%s/%.*s/RESERVED", USERS_DIR, UID_LEN, uid);
    
    struct dirent **namelist;
    int n = scandir(dir_path, &namelist, NULL, dirent_alphasort);
    if (n < 0) return -1;

    buffer[0] = '\0';
    int count = 0;

    // Iterate in reverse to get most recent? 
    // Spec says "most recent 50 reservations".
    // Filenames are R-UID-YYYYMMDDHHMMSS.txt. dirent_alphasort keeps date order.
    // So we should iterate from end to beginning.
    
    for (int i = n - 1; i >= 0; i--) {
        if (namelist[i]->d_name[0] != '.') {
            if (count >= 50) {
                free(namelist[i]);
                continue;
            }

            char path[PATH_MAX_LEN];
            size_t dir_len = strlen(dir_path);
            size_t name_len = strnlen(namelist[i]->d_name, sizeof(namelist[i]->d_name));
            size_t needed = dir_len + 1 + name_len + 1; // dir + '/' + name + '\0'
            if (needed >= sizeof(path)) {
                free(namelist[i]);
                continue; // Skip overly long path
            }
            memcpy(path, dir_path, dir_len);
            path[dir_len] = '/';
            memcpy(path + dir_len + 1, namelist[i]->d_name, name_len);
            path[dir_len + 1 + name_len] = '\0';
            FILE *fp = fopen(path, "r");
            if (fp) {
                char r_uid[7], r_date[20];
                int r_num;
                char r_time[10];
                fscanf(fp, "%s %d %s %s", r_uid, &r_num, r_date, r_time);
                fclose(fp);
                char found_eid[4] = "";
                struct dirent **eventlist;
                int num_events = scandir(EVENTS_DIR, &eventlist, NULL, dirent_alphasort);
                if (num_events >= 0) {
                    for (int j = 0; j < num_events; j++) {
                        if (eventlist[j]->d_name[0] != '.') {
                            char check_path[PATH_MAX_LEN];
                            snprintf(check_path, sizeof(check_path), "%s/%.*s/RESERVATIONS/%.255s", 
                                     EVENTS_DIR, EID_LEN, eventlist[j]->d_name, namelist[i]->d_name);
                            if (access(check_path, F_OK) == 0) {
                                strncpy(found_eid, eventlist[j]->d_name, 3);
                                found_eid[3] = '\0';
                                free(eventlist[j]);
                                // Free remaining
                                for (int k = j + 1; k < num_events; k++) free(eventlist[k]);
                                break;
                            }
                        }
                        free(eventlist[j]);
                    }
                    free(eventlist);
                }
                
                if (found_eid[0] != '\0') {
                    char entry[100];
                    snprintf(entry, sizeof(entry), " %s %s %s %d", found_eid, r_date, r_time, r_num);
                    if (strlen(buffer) + strlen(entry) < max_len) {
                        strcat(buffer, entry);
                        count++;
                    }
                }
            }
        }
        free(namelist[i]);
    }
    free(namelist);
    return 0;
}

int storage_get_event_details(const char *eid, char *uid_out, char *name_out, char *date_out, 
                             char *time_out, int *attendance_out, int *reserved_out, 
                             char *fname_out, size_t *fsize_out) {
    
    char start_path[PATH_MAX_LEN];
    snprintf(start_path, sizeof(start_path), "%s/%.*s/START%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
    FILE *fp = fopen(start_path, "r");
    if (!fp) return -1;

    fscanf(fp, "%s %s %s %d %s %s", uid_out, name_out, fname_out, attendance_out, date_out, time_out);
    fclose(fp);

    *reserved_out = get_reservations_count(eid);

    // Get file size
    char file_path[PATH_MAX_LEN];
    snprintf(file_path, sizeof(file_path), "%s/%.*s/DESCRIPTION/%.*s", EVENTS_DIR, EID_LEN, eid, FNAME_MAX_LEN, fname_out);
    struct stat st;
    if (stat(file_path, &st) == 0) {
        *fsize_out = st.st_size;
    } else {
        *fsize_out = 0;
    }
    
    // Check if we need to close it (lazy close)
    if (!storage_is_event_closed(eid) && is_date_past(date_out, time_out)) {
        char end_path[PATH_MAX_LEN];
        snprintf(end_path, sizeof(end_path), "%s/%.*s/END%.*s.txt", EVENTS_DIR, EID_LEN, eid, EID_LEN, eid);
        FILE *end_fp = fopen(end_path, "w");
        if (end_fp) {
            fprintf(end_fp, "%s %s:00", date_out, time_out);
            fclose(end_fp);
        }
    }

    return 0;
}

int storage_get_event_file_data(const char *eid, const char *fname, void *buffer, size_t size) {
    char file_path[PATH_MAX_LEN];
    snprintf(file_path, sizeof(file_path), "%s/%.*s/DESCRIPTION/%.*s", EVENTS_DIR, EID_LEN, eid, FNAME_MAX_LEN, fname);
    FILE *fp = fopen(file_path, "rb");
    if (!fp) return -1;
    size_t read = fread(buffer, 1, size, fp);
    fclose(fp);
    return (read == size) ? 0 : -1;
}
