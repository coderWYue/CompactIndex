#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>

// #define LOG_DEBUG

enum log_level { DEBUG = 0, INFO, WARNING, ERROR, UPDATE, APPEND, LOG_TYPE_NUM};

static enum log_level this_log_level = DEBUG;

static const char *log_level_str[] = { "DEBUG", "INFO", "WARNING", "ERROR", "UPDATE", "APPEND"};

#ifdef LOG_DEBUG
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "[%s:%u] %s: " fmt  "\n", __FILE__, __LINE__, \
				level_str, ##__VA_ARGS__);
#else
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "%s: " fmt "\n", level_str, ##__VA_ARGS__);
#endif

#define log_it_to_file(fd, fmt, level_str, ...) \
    fprintf(fd, "%s: " fmt "\n", level_str, ##__VA_ARGS__);

#define log_it_to_buf(buf, fmt, level_str, ...) \
    sprintf(buf, "%s: " fmt "\n", level_str, ##__VA_ARGS__);

#define log_to_buf(buf, level, fmt, ...) \
	do { \
		if (level < this_log_level || level >= LOG_TYPE_NUM) \
			break; \
		log_it_to_buf(buf, fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)

#define log_to_file(fd, level, fmt, ...) \
	do { \
		if (level < this_log_level || level >= LOG_TYPE_NUM) \
			break; \
		log_it_to_file(fd, fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)

#define log(level, fmt, ...) \
	do { \
		if (level < this_log_level || level >= LOG_TYPE_NUM) \
			break; \
		log_it(fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)

#endif
