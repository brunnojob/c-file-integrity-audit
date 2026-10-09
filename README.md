# File Integrity Audit

A recursive SHA-256 file inventory with size comparison and detection of additions, changes, and removals.

## Run

Requirements: C17 and OpenSSL.

```sh
make
build/integrity snapshot ./arquivos > ./baseline.txt
build/integrity verify ./arquivos ./baseline.txt > result.json
```

## Behavior

Keep the manifest outside the audited directory. Symbolic links and special files are rejected. Exit codes: 0 for an exact match, 1 for differences, and 2 for an error. Keep the baseline manifest under your control.

## Result synchronization

The [operations archive](https://vercel-home-telemetry-api.vercel.app/laboratory.html?project=c-file-integrity-audit) stores execution results. Supabase migrations are in the [API repository](https://github.com/brunnojob/vercel-home-telemetry-api/tree/main/supabase/migrations).

```sh
python cloud/sync.py enqueue result.json --project c-file-integrity-audit
python cloud/sync.py sync
```

Set `BRUNNODEV_ACCESS_TOKEN` to your session token. The SQLite outbox retains reports until the server confirms persistence; identical content does not create duplicate records. Tokens are not stored in source code. To run the synchronization tests:

```sh
python -m unittest discover -s cloud
```
