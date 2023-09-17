#include "../include/coroutine.h"

/* converted from https://www.geeksforgeeks.org/reflection-in-golang/ */

// Example program to show difference between
// Type and Kind and to demonstrate use of
// Methods provided by Go reflect Package

reflect_struct(details,
               (STRING, string, fname),
               (STRING, string, lname),
               (INTEGER, int, age),
               (FLOAT, float, balance)
)

typedef string myType;

void showDetails(reflect_type_t *i, void_t j) {
    char value[20] = {0};
    reflect_field_t *val = reflect_value_of(i);

    string_t t1 = reflect_type_of(i);
    string_t k1 = reflect_kind(i);

    string_t t2 = reflect_type_of(j);
    string_t k2 = reflect_kind(j);

    printf("Type of first interface: %s\n", t1);
    printf("Kind of first interface: %s\n", k1);
    printf("Type of second interface: %s\n", t2);
    printf("Kind of second interface: %s\n", k2);

    printf("\nThe values in the first argument are :\n");
    if (strcmp(reflect_kind(i), "struct") == 0) {
        size_t numberOfFields = reflect_num_fields(i);
        for (int index = 0; index < numberOfFields; index++) {
            reflect_get_field(i, index, value);
            if (index == 3 || index == 0)
                printf("%d.Type: %s || Value: %d\n",
                   (index + 1), reflect_field_type(i, index), c_int(value));
            else if (index == 4)
                printf("%d.Type: %s || Value: %f\n",
                   (index + 1), reflect_field_type(i, index), c_float(value));
            else
                printf("%d.Type: %s || Value: %s\n",
                   (index + 1), reflect_field_type(i, index), c_char_ptr(value));

            printf("Kind is %s\n", reflect_kind(val + index));
        }
    }

    reflect_get_field(j, 1, value);
    printf("\nThe Value passed in second parameter is %s\n", c_char_ptr(value));
}

int co_main(int argc, char **argv) {
    as_var_ref(iD, myType, "12345678", CO_STRING);

    as_instance_ref(person, details);
    person->fname = co_strdup("Go");
    person->lname = co_strdup("Geek");
    person->age = 32;
    person->balance = 33000.203;

    showDetails(person_r, iD_r);

    return 0;
}
