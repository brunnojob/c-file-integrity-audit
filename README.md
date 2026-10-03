# c-file-integrity-audit

C utility for SHA-256 file integrity checks using OpenSSL EVP. Generate a digest with `./audit hash archive.zip`; verify by piping the saved 64-character digest to `./audit verify archive.zip`.

Build: `cc -std=c17 -Wall -Wextra -Werror src/main.c -lcrypto -o audit`.

Project by [Brunno Dev](https://brunnodev.store).