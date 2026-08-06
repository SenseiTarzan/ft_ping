//
// Created by gcaptari on 05/08/2026.
//

#include "ping.h"

#include <stdlib.h>


t_sequence_ping_history *sequence_ping_history_new(uint16_t sequence) {
    t_sequence_ping_history *history;
    history = (t_sequence_ping_history *)malloc(sizeof(t_sequence_ping_history));
    if (history == NULL) return (NULL);
    history->sequence = sequence;
    history->next = NULL;
    return (history);
}

bool sequence_ping_history_has(const t_sequence_ping_history *history, uint16_t sequence) {
    for (const t_sequence_ping_history *h = history; h != NULL; h = h->next) {
        if (h->sequence == sequence) return (true);
    }
    return (false);
}

bool sequence_ping_history_add(t_sequence_ping_history **history, uint16_t sequence) {
    if (history == NULL) return (false);
    if (*history == NULL) {
        *history = sequence_ping_history_new(sequence);
        if (*history == NULL) return (false);
        return (true);
    }
    t_sequence_ping_history *new_history = sequence_ping_history_new(sequence);
    if (new_history == NULL) return (false);
    t_sequence_ping_history *h = *history;
    while (h->next != NULL){
        h = h->next;
    }
    h->next = new_history;
    return (true);
}

bool sequence_ping_history_free(t_sequence_ping_history *history) {
    if (history == NULL) return (false);
    t_sequence_ping_history *h = history;
    while (h != NULL) {
        t_sequence_ping_history *old_history = h;
        h = h->next;
        free(old_history);
    }
    return (true);
}