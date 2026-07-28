# Installing and linking

The local Core development package contains `include/`, `lib/`, `examples/`,
and this documentation. It is a Windows x86_64 release-candidate package.

Compile the independent example from the extracted package root:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic examples\external_consumer.c -Iinclude lib\libminisnn_core.a -o external_consumer.exe
.\external_consumer.exe
```

Use only headers in `include/`; `src/`, `app/`, `studio/`, and test hooks are
not distributable API. The static library is toolchain-specific: binary ABI
compatibility across arbitrary Windows toolchains is not guaranteed. The public
surface is `CANDIDATE_V1` and remains provisional until D2.
