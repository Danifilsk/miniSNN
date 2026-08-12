#define main k2_a_snapshot_fixture_main
#include "test_k2_a_snapshot.c"
#undef main

#include "k2_snapshot_file.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int create_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0700) == 0;
#endif
}

static int remove_directory(const char *path)
{
#ifdef _WIN32
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        return 0;
    }
    return fclose(file) == 0;
}

static void assert_file_matches_snapshot(
    const char *path,
    const MiniSNNWorldsKernelSnapshot *snapshot)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    size_t size;

    REQUIRE(file != NULL);
    REQUIRE(fseek(file, 0L, SEEK_END) == 0);
    REQUIRE(ftell(file) >= 0L);
    size = (size_t)ftell(file);
    REQUIRE(fseek(file, 0L, SEEK_SET) == 0);
    REQUIRE(size == minisnn_worlds_kernel_snapshot_size(snapshot));
    data = malloc(size);
    REQUIRE(data != NULL);
    REQUIRE(fread(data, 1U, size, file) == size);
    REQUIRE(fclose(file) == 0);
    REQUIRE(memcmp(data, minisnn_worlds_kernel_snapshot_data(snapshot), size) == 0);
    free(data);
}

int main(void)
{
    static const char snapshot_path[] = "k2_b_roundtrip.bin";
    static const char empty_path[] = "k2_b_empty.bin";
    static const char replace_failure_path[] = "k2_b_replace_failure";
    static const char replace_failure_temp_path[] = "k2_b_replace_failure.tmp";
    MiniSNNWorldsKernel *source = build_full_state();
    MiniSNNWorldsKernel *restored = NULL;
    MiniSNNWorldsKernelSnapshot *captured = NULL;
    MiniSNNWorldsKernelSnapshot *loaded = NULL;
    MiniSNNWorldsKernelSnapshot *after_restore = NULL;
    FILE *empty;

    REQUIRE(minisnn_worlds_kernel_snapshot_capture(source, &captured) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_save_file(snapshot_path, captured) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    assert_file_matches_snapshot(snapshot_path, captured);
    REQUIRE(minisnn_worlds_kernel_snapshot_load_file(snapshot_path, &loaded) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(loaded != NULL);
    REQUIRE(minisnn_worlds_kernel_snapshot_size(captured) ==
            minisnn_worlds_kernel_snapshot_size(loaded));
    REQUIRE(memcmp(minisnn_worlds_kernel_snapshot_data(captured),
                   minisnn_worlds_kernel_snapshot_data(loaded),
                   minisnn_worlds_kernel_snapshot_size(captured)) == 0);
    REQUIRE(minisnn_worlds_kernel_create_from_snapshot(loaded, &restored) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(minisnn_worlds_kernel_snapshot_capture(restored, &after_restore) ==
            MINISNN_WORLDS_KERNEL_ERROR_NONE);
    REQUIRE(memcmp(minisnn_worlds_kernel_snapshot_data(captured),
                   minisnn_worlds_kernel_snapshot_data(after_restore),
                   minisnn_worlds_kernel_snapshot_size(captured)) == 0);

    REQUIRE(minisnn_worlds_kernel_snapshot_save_file(NULL, captured) ==
            MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    minisnn_worlds_kernel_snapshot_destroy(loaded);
    loaded = NULL;
    REQUIRE(minisnn_worlds_kernel_snapshot_load_file(NULL, &loaded) ==
            MINISNN_WORLDS_KERNEL_ERROR_NULL_ARGUMENT);
    empty = fopen(empty_path, "wb");
    REQUIRE(empty != NULL);
    REQUIRE(fclose(empty) == 0);
    loaded = NULL;
    REQUIRE(minisnn_worlds_kernel_snapshot_load_file(empty_path, &loaded) ==
            MINISNN_WORLDS_KERNEL_ERROR_SNAPSHOT_INVALID_FORMAT);
    REQUIRE(loaded == NULL);

    REQUIRE(create_directory(replace_failure_path));
    REQUIRE(minisnn_worlds_kernel_snapshot_save_file(replace_failure_path, captured) ==
            MINISNN_WORLDS_KERNEL_ERROR_INTERNAL);
    REQUIRE(!file_exists(replace_failure_temp_path));
    REQUIRE(remove_directory(replace_failure_path));

    REQUIRE(remove(empty_path) == 0);
    REQUIRE(remove(snapshot_path) == 0);
    minisnn_worlds_kernel_snapshot_destroy(after_restore);
    minisnn_worlds_kernel_destroy(restored);
    minisnn_worlds_kernel_snapshot_destroy(loaded);
    minisnn_worlds_kernel_snapshot_destroy(captured);
    minisnn_worlds_kernel_destroy(source);
    puts("K2-B binary save/load round-trip OK");
    return 0;
}