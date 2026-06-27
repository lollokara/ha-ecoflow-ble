#ifndef REMOTE_LOGGER_H
#define REMOTE_LOGGER_H

// Forward an already-parsed log line (level + tag + message) to the STM32 over
// the inter-chip UART. Only important lines are actually transmitted; see the
// implementation for the filtering rules.
void RemoteLogger_Forward(int level, const char* tag, const char* msg);

#endif
