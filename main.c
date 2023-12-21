/*
 * Copyright 2013, 2014 Chris Rhodin <chris@notav8.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "project.h"

#include "timer.h"
#include "tick.h"

#include <stdio.h>
#include <stdlib.h>
#include <util/atomic.h>

#include <avr/pgmspace.h>

uint8_t volatile events = 0;

uint16_t volatile count = 0;

static int8_t scan_key_handler(struct timer_event * this_timer_event)
{
    count++;

    /* set event to enable key scan */
    events |= 0x01;

    /* advance this timer 5 milliseconds */
    this_timer_event->tbtick += TBTICKS_FROM_MS(5);

    /* reschedule this timer */
    return 1;
}

static struct timer_event scan_key_event = {
    .next = &scan_key_event,
    .handler = scan_key_handler,
};


void main(void)
{
    // initialize
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        tbtick_init();
        tick_init();
    }
    // interrupts are enabled

    for (;;);
}

