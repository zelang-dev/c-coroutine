
#include <uv.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <openssl/pem.h>
#include <openssl/x509.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <compat/unistd.h>
#else
    #include <unistd.h>
#endif


void str_ext(char *buffer, const char *name, const char *ext) {
    int r = snprintf(buffer, strlen(buffer), "%s%s", name, ext);
#ifdef _WIN32
#else
    if (r)
        (void *)r;
#endif
}

/* Generates a 4096-bit RSA key. */
EVP_PKEY *generate_key() {
    /* Allocate memory for the EVP_PKEY structure. */
    EVP_PKEY *pkey = EVP_PKEY_new();
    if (!pkey) {
        puts("Unable to create EVP_PKEY structure.");
        return NULL;
    }

    /* Generate the RSA key and assign it to pkey. */
    RSA *rsa = RSA_generate_key(4096, RSA_F4, NULL, NULL);
    if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
        puts("Unable to generate 4096-bit RSA key.");
        EVP_PKEY_free(pkey);
        return NULL;
    }

    /* The key has been generated, return it. */
    return pkey;
}

/* Generates a self-signed x509 certificate. */
X509 *generate_x509(EVP_PKEY *pkey, const char * country, const char * org, const char * domain) {
    /* Allocate memory for the X509 structure. */
    X509 *x509 = X509_new();
    if (!x509) {
        puts("Unable to create X509 structure.");
        return NULL;
    }

    /* Set the serial number. */
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    /* This certificate is valid from now until exactly one year from now. */
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);

    /* Set the public key for our certificate. */
    X509_set_pubkey(x509, pkey);

    /* We want to copy the subject name to the issuer name. */
    X509_NAME *name = X509_get_subject_name(x509);

    /* Set the country code and common name. */
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (const unsigned char *)(country == NULL ? "US" : country), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (const unsigned char *)(org == NULL ? "selfSigned" : org), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)(domain == NULL ? "localhost" : domain), -1, -1, 0);

    /* Now set the issuer name. */
    X509_set_issuer_name(x509, name);

    /* Actually sign the certificate with our key. */
    if (!X509_sign(x509, pkey, EVP_sha256())) {
        puts("Error signing certificate.");
        X509_free(x509);
        return NULL;
    }

    return x509;
}

int write_to_disk(EVP_PKEY *pkey, X509 *x509, const char * hostname) {
    /* Open the PEM file for writing the key to disk. */
    char key[256];
    char crt[256];
    str_ext(key, hostname, ".key");
    FILE *pkey_file = fopen(key, "wb");
    if (!pkey_file) {
        printf("Unable to open \"%s\" for writing.\n", key);
        return -1;
    }

    /* Write the key to disk. */
    int ret = PEM_write_PrivateKey(pkey_file, pkey, NULL, NULL, 0, NULL, NULL);
    fclose(pkey_file);

    if (!ret) {
        puts("Unable to write private key to disk.");
        return -1;
    }

    /* Open the PEM file for writing the certificate to disk. */
    str_ext(crt, hostname, ".crt");
    FILE *x509_file = fopen(crt, "wb");
    if (!x509_file) {
        printf("Unable to open \"%s\" for writing.\n", crt);
        return -1;
    }

    /* Write the certificate to disk. */
    ret = PEM_write_X509(x509_file, x509);
    fclose(x509_file);

    if (!ret) {
        puts("Unable to write certificate to disk.");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    char name[256];
    size_t len = sizeof(name);
    uv_os_gethostname(name, &len);

    /* Generate the key. */
    puts("Generating RSA key...");
    EVP_PKEY *pkey = generate_key();
    if (!pkey) {
        return 1;
    }

    /* Generate the certificate. */
    puts("Generating x509 certificate...");
    X509 *x509 = generate_x509(pkey, NULL, NULL, name);
    if (!x509) {
        EVP_PKEY_free(pkey);
        return 1;
    }

    /* Write the private key and certificate out to disk. */
    puts("Writing key and certificate to disk...");
    int ret = write_to_disk(pkey, x509, name);
    EVP_PKEY_free(pkey);
    X509_free(x509);

    if (ret) {
        puts("Success!");
        return 0;
    } else
        return 1;
}
