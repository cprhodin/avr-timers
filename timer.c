/*
 * Copyright 2013-2023 Chris Rhodin <chris@notav8.com>
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

#include "pinmap.h"
#include "timer.h"

#include <util/atomic.h>
#include <avr/interrupt.h>
#include <stdio.h>

/*
 * system timebase
 */
static tbtick_t tbtick_counter;
static struct timer_event * timer_event_list;

#if (TBTIMER == 0)
tbtick_t tbtick_update(void) __attribute__((__naked__));
tbtick_t tbtick_update(void)
{
    register tbtick_t tick __asm__("r22");

    __asm__ __volatile__ (
        "               in              r22, %[tbtcnt]-0x20                  \n"
        "               ldi             r30, lo8(tbtick_counter)             \n"
        "               ldi             r31, hi8(tbtick_counter)             \n"
        "               ldd             __tmp_reg__, Z+0                     \n"
        "               ldd             r23, Z+1                             \n"
        "               ldd             r24, Z+2                             \n"
        "               ldd             r25, Z+3                             \n"
        "               cp              r22, __tmp_reg__                     \n"
        "               adc             r23, __zero_reg__                    \n"
        "               adc             r24, __zero_reg__                    \n"
        "               adc             r25, __zero_reg__                    \n"
        "               std             Z+0, r22                             \n"
        "               std             Z+1, r23                             \n"
        "               std             Z+2, r24                             \n"
        "               std             Z+3, r25                             \n"
        "                                                                    \n"
        "               ret                                                  \n"
        :
        : /* i/o registers */
          [tbtick_counter] "X" (&tbtick_counter),
          [tbtcnt] "i" _SFR_MEM_ADDR(TBTCNT)
        : "r0", "r1", "r22", "r23", "r24", "r25", "r30", "r31", "memory"
    );

    return tick;
}
#elif (TBTIMER == 1)
tbtick_t tbtick_update(void) __attribute__((__naked__));
tbtick_t tbtick_update(void)
{
    register tbtick_t tick __asm__("r22");

    __asm__ __volatile__ (
        "               lds             r22, %[tbtcntl]                      \n"
        "               lds             r23, %[tbtcnth]                      \n"
        "               ldi             r30, lo8(tbtick_counter)             \n"
        "               ldi             r31, hi8(tbtick_counter)             \n"
        "               ldd             r24, Z+2                             \n"
        "               ldd             r25, Z+3                             \n"
        "               ld              __tmp_reg__, Z                       \n"
        "               cp              r22, __tmp_reg__                     \n"
        "               ldd             __tmp_reg__, Z+1                     \n"
        "               cpc             r23, __tmp_reg__                     \n"
        "               adc             r24, __zero_reg__                    \n"
        "               adc             r25, __zero_reg__                    \n"
        "               std             Z+0, r22                             \n"
        "               std             Z+1, r23                             \n"
        "               std             Z+2, r24                             \n"
        "               std             Z+3, r25                             \n"
        "                                                                    \n"
        "               ret                                                  \n"
        :
        : /* i/o registers */
          [tbtick_counter] "X" (&tbtick_counter),
          [tbtcntl] "i" _SFR_MEM_ADDR(TBTCNTL),
          [tbtcnth] "i" _SFR_MEM_ADDR(TBTCNTH)
        : "r0", "r1", "r22", "r23", "r24", "r25", "r30", "r31", "memory"
    );

    return tick;
}
#elif (TBTIMER == 2)
tbtick_t tbtick_update(void) __attribute__((__naked__));
tbtick_t tbtick_update(void)
{
    register tbtick_t tick __asm__("r22");

    __asm__ __volatile__ (
        "               lds             r22, %[tbtcnt]                       \n"
        "               ldi             r30, lo8(tbtick_counter)             \n"
        "               ldi             r31, hi8(tbtick_counter)             \n"
        "               ldd             __tmp_reg__, Z+0                     \n"
        "               ldd             r23, Z+1                             \n"
        "               ldd             r24, Z+2                             \n"
        "               ldd             r25, Z+3                             \n"
        "               cp              r22, __tmp_reg__                     \n"
        "               adc             r23, __zero_reg__                    \n"
        "               adc             r24, __zero_reg__                    \n"
        "               adc             r25, __zero_reg__                    \n"
        "               std             Z+0, r22                             \n"
        "               std             Z+1, r23                             \n"
        "               std             Z+2, r24                             \n"
        "               std             Z+3, r25                             \n"
        "                                                                    \n"
        "               ret                                                  \n"
        :
        : /* i/o registers */
          [tbtick_counter] "X" (&tbtick_counter),
          [tbtcnt] "i" _SFR_MEM_ADDR(TBTCNT)
        : "r0", "r1", "r22", "r23", "r24", "r25", "r30", "r31", "memory"
    );

    return tick;
}
#endif


static void link_timer_event(struct timer_event * this_timer_event)
{
    struct timer_event ** tthis_timer_event;

    for ( tthis_timer_event  = &timer_event_list;
         *tthis_timer_event != NULL;
          tthis_timer_event  = &((*tthis_timer_event)->next))
    {
        if ((tbtick_st)
            (this_timer_event->tbtick - (*tthis_timer_event)->tbtick) < 0)
        {
            break;
        }
    }

    this_timer_event->next = *tthis_timer_event;
    *tthis_timer_event = this_timer_event;
}


static void unlink_timer_event(struct timer_event * this_timer_event)
{
    struct timer_event ** tthis_timer_event;

    for ( tthis_timer_event  = &timer_event_list;
         *tthis_timer_event != NULL;
          tthis_timer_event  = &((*tthis_timer_event)->next))
    {
        if (*tthis_timer_event == this_timer_event)
        {
            *tthis_timer_event = this_timer_event->next;
            break;
        }
    }

    this_timer_event->next = this_timer_event;
}


/*
 * timebase interrupt handler
 */
static void tbtimer_handler(void)
{
    for (;;)
    {
        tbtick_st delta;
        tbtimer_t ocr;

        if (timer_event_list == NULL)
        {
            /* no pending timer events */
            delta = TBTIMER_MAX_DELAY;
        }
        else
        {
            /* process timer event */
            delta = timer_event_list->tbtick - tbtick_update();

            if (delta < 0)
            {
                /* handle expired timer event */
                struct timer_event * this_timer_event;

                this_timer_event = timer_event_list;
                timer_event_list = this_timer_event->next;
                this_timer_event->next = this_timer_event;

                if (this_timer_event->handler)
                {
                    if (this_timer_event->handler(this_timer_event))
                    {
                        link_timer_event(this_timer_event);
                    }
                }

                continue;
            }

            if (delta > TBTIMER_MAX_DELAY)
            {
                /* limit delta to maximum supported by timer */
                delta = TBTIMER_MAX_DELAY;
            }
        }

        TBTOCR = ocr = (tbtimer_t) tbtick_update() + delta + 1;

        if ((tbtimer_st) (TBTCNT - ocr) < 0)
        {
            break;
        }
    }
}


ISR(TBTIMER_COMP_vect)
{
    tbtimer_handler();
}


void tbtimer_delay(tbtimer_st counts)
{
    tbtimer_t terminal;

    terminal = tbtimer_get() + counts;

    while ((tbtimer_st) (terminal - tbtimer_get()) >= 0);
}


void tbtick_delay(tbtick_st counts)
{
    tbtick_t terminal;

    terminal = tbtick_get() + counts;

    while ((tbtick_st) (terminal - tbtick_get()) >= 0);
}


void schedule_timer_event(struct timer_event * this_timer_event,
                          struct timer_event * ref_timer_event)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        this_timer_event->tbtick += (ref_timer_event)
                                  ? (ref_timer_event->tbtick)
                                  : (tbtick_update());

        link_timer_event(this_timer_event);

        tbtimer_handler();
    }
}


void cancel_timer_event(struct timer_event * this_timer_event)
{
    if (!timer_is_expired(this_timer_event)) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            unlink_timer_event(this_timer_event);

            tbtimer_handler();
        }
    }
}


void timer_delay(tbtick_st ticks)
{
    struct timer_event timer_delay_event;

    init_timer_event(&timer_delay_event, ticks, NULL);

    schedule_timer_event(&timer_delay_event, NULL);

    while (!timer_is_expired(&timer_delay_event));
}


/*
 * setup timer as a free running counter
 */
void tbtick_init(void)
{
    /*
     * initialize timer
     */

    /* setup the initial timer interrupt */
    TBTOCR = TBTIMER_MAX_DELAY;

    /*
     * initialize timebase
     */
    tbtick_counter = 0;
    timer_event_list = NULL;

    /* clear pending timer interrupts */
    TBTIFR = _BV(TBTOCF);

    /* enable compare interrupt */
    TBTIMSK |= _BV(TBTOCIE);
}

