/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

 /*
 *	Name: main.c
 *  Desctiption: This file demonstrates the use of mmap to map physical addresses of hardware peripheral registers
 *  			 to the virtual address space of the process. This capability has been used to test the PWM functionality.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"
#include "rpihw.h"
#include "mailbox.h"

#include "ws2811.h"

#define DEV_MEM "/dev/mem" // Device memory 
#define OSC_FREQ 19200000 // Raspberry Pi frequency
#define OSC_FREQ_PI4 54000000 // Raspberry Pi 4 frequency

// Structure to hold device pointers
typedef struct {
	// Pointer to Raspberry Pi hardware information
	const rpi_hw_t *rpi_hw;

	// Pointers to structures that are mapped to Physical registers
	volatile pwm_t *pwm;
	volatile gpio_t *gpio;
	volatile cm_clk_t *cm_clk;
} test_device_t;

// Enum for duty_cycle
typedef enum {
	INCREASE,
	DECREASE,
} duty_cycle_change_t;

// Variables to store the GPIO, Channel and Duty cycle parameters received from the command line
int gpio_num;
int channel_num;
int duty_cycle;

// Flags to check if command line parameters have been provided
int gpio_flag = 0;
int channel_flag = 0;
int duty_cycle_flag = 0;

uint32_t running = 1; // Running flag

// Function declarations
void print_usage();
int map_registers_pwm(test_device_t *test_device);
int unmap_registers_pwm(test_device_t *test_device);
int check_pin_setup(test_device_t *test_device);
void pwm_register_config(test_device_t *test_device);
void set_pwm_max(test_device_t *test_device, uint32_t range);
void enable_pwm(test_device_t *test_device);
void disble_pwm(test_device_t *test_device);
void pwm_set_duty_cycle(test_device_t *test_device);
void pwm_duty_cycle_change(test_device_t *test_device, int percentage, duty_cycle_change_t change);


// Function to parse arguments
void parse_args(int argc, char **argv)
{
	// Short options string
    const char *short_options = "h";
    
    // Options struct containing user defined long options "name" and "alg"
    static struct option long_options[] =
    {
		{"gpio", required_argument, NULL, 'g'},
		{"channel", required_argument, NULL, 'c'},
		{"duty_cycle", required_argument, NULL, 'd'},
        {0, 0, 0, 0}
    };

	int ch;

	// Local variables for GPIO, Channel and duty_cycle
	int gpio, channel, d_cycle;

	printf("Test\n");
    
    // Iterating until getopt_long() returns -1
    while ((ch = getopt_long(argc, argv, short_options, long_options, NULL )) != -1) {
      switch (ch) {
		case 'g':
			gpio_flag = 1;
			gpio = atoi(optarg);
            break;
		case 'd':
			duty_cycle_flag = 1;
			d_cycle = atoi(optarg);
            break;			
		case 'c':
			channel_flag = 1;
			channel = atoi(optarg);
            break;
		case 'h':
			print_usage();
            break;
          case '?':
              /* getopt_long already printed an error message. */
              break;
          default:
              break;
      }
    }

	// Checking if the gpio flag was set
	if (gpio_flag) {
		gpio_num = gpio;
	} else {
		gpio_num = 18; // Default pin number is 18
	}

	// Checking if the channel flag was set
	if (channel_flag) {
		channel_num = channel;
	} else {
		channel_num = 0; // Default channel number is 0
	}

	// Checking if the duty cycle flag was set
	if ((duty_cycle_flag) && (d_cycle >= 0 && d_cycle <= 100)) {
		duty_cycle = d_cycle;
	} else {
		duty_cycle = 50; // Default duty cycle is 50%
	}
}

// This function is maps the hardware registers to enable PWM functionality
int map_registers_pwm(test_device_t *test_device)
{
	uint32_t base = test_device->rpi_hw->periph_base;
	// The mapmem function contains an mmap function call with parameters provided
	test_device->pwm = mapmem(PWM_OFFSET + base, sizeof(pwm_t), DEV_MEM);
	if (!test_device->pwm)
	{
		return -1;
	}

	test_device->gpio = mapmem(GPIO_OFFSET + base, sizeof(gpio_t), DEV_MEM);
    if (!test_device->gpio)
    {
        return -1;
    }

	test_device->cm_clk = mapmem(CM_PWM_OFFSET + base, sizeof(cm_clk_t), DEV_MEM);
    if (!test_device->cm_clk)
    {
        return -1;
    }
	return 0;
}

// Function to unmap registers after their usage is complete
int unmap_registers_pwm(test_device_t *test_device)
{
	if (test_device->pwm) {
		unmapmem((void *)test_device->pwm, sizeof(pwm_t));
	}

	if (test_device->gpio) {
		unmapmem((void *)test_device->gpio, sizeof(gpio_t));
	}

	if (test_device->cm_clk) {
		unmapmem((void *)test_device->cm_clk, sizeof(cm_clk_t));
	}
}

// Function to print the usage for this executable
void print_usage()
{
	printf("The program execution is as follows:\n");
	printf("./pwm_test --gpio=[GPIO Number] --channel=[Channel Number] --duty_cycle=[Duty Cycle] -h\n");
}

// Function to check if the pin setup is valid based on the user inputs and sets the required GPIO functionality
int check_pin_setup(test_device_t *test_device)
{
	int altnum = pwm_pin_alt(channel_num, gpio_num);

	if (altnum == -1) {
		printf("Invalid GPIO or Channel Number\n");
		return -1;
	}

	gpio_function_set(test_device->gpio, gpio_num, altnum);
	return 0;
}

// Configuration function that accesses registers and sets up the PWM
void pwm_register_config(test_device_t *test_device)
{
	volatile pwm_t *pwm = test_device->pwm; 
	volatile cm_clk_t *cm_clk = test_device->cm_clk;

	// Turn off the PWM in case already running
    pwm->ctl = 0;
    usleep(10);

    // Stopping the clock if its running
    cm_clk->ctl = CM_CLK_CTL_PASSWD | CM_CLK_CTL_KILL;
    usleep(10);
    while (cm_clk->ctl & CM_CLK_CTL_BUSY);

	uint32_t osc_freq = OSC_FREQ;

    if(test_device->rpi_hw->type == RPI_HWVER_TYPE_PI4){
        osc_freq = OSC_FREQ_PI4;
    }

	// WS2811 frequency of 800kHz; reused for clock scaling
    uint32_t freq = 800000;

    // Setup the Clock - Use OSC @ 19.2Mhz w/ 3 clocks/tick
    cm_clk->div = CM_CLK_DIV_PASSWD | CM_CLK_DIV_DIVI(osc_freq / (3 * freq));
    cm_clk->ctl = CM_CLK_CTL_PASSWD | CM_CLK_CTL_SRC_OSC;
    cm_clk->ctl = CM_CLK_CTL_PASSWD | CM_CLK_CTL_SRC_OSC | CM_CLK_CTL_ENAB;
    usleep(10);
    while (!(cm_clk->ctl & CM_CLK_CTL_BUSY));
}

// Sets the maximum value for the PWM signal; this value is the basis for the duty cycle
void set_pwm_max(test_device_t *test_device, uint32_t range)
{
	test_device->pwm->rng1 = range;
	usleep(10);
}

// Enable pwm based on the channel number
void enable_pwm(test_device_t *test_device)
{
	if (!channel_num)
		test_device->pwm->ctl |= RPI_PWM_CTL_PWEN1;
	else
		test_device->pwm->ctl |= RPI_PWM_CTL_PWEN2;
	usleep(10);
}

// Disable pwm based on the channel number
void disable_pwm(test_device_t *test_device)
{
	if (!channel_num)
		test_device->pwm->ctl &= ~RPI_PWM_CTL_PWEN1;
	else
		test_device->pwm->ctl &= ~RPI_PWM_CTL_PWEN2;
	usleep(10);
}

// Setting the PWM duty cycle
void pwm_set_duty_cycle(test_device_t *test_device)
{
	uint32_t range;
	if (!channel_num) {
		range = test_device->pwm->rng1;
		test_device->pwm->dat1 = (range*duty_cycle)/100;
	}
	else {
		range = test_device->pwm->rng2;
		test_device->pwm->dat2 = (range*duty_cycle)/100;
	}
	usleep(10);
}

// Changing the PWM duty cycke based on the percentage and type of change provided
void pwm_duty_cycle_change(test_device_t *test_device, int percentage, duty_cycle_change_t change)
{
	volatile pwm_t *pwm = test_device->pwm;
	uint32_t range;
	uint32_t data_register;
	uint32_t current_duty_cycle;

	// If channel 1 is enabled
	if (pwm->ctl & RPI_PWM_CTL_PWEN1) {
		range = pwm->rng1;
		data_register = pwm->dat1;
		current_duty_cycle = (data_register * 100)/range;
		printf("Current duty cycle is %u\n", current_duty_cycle);
		
		uint32_t change_value = (percentage * range) / 100;

		// Checking the type of change
		if (change == INCREASE) {
			// Checking if the current duty cycle will exceed the maximum value
			if (data_register + change_value > range)
				return;
			pwm->dat1 += change_value;
		}
		else if (change == DECREASE){
			// Checking if the current duty cycle will fall below the minimum value
			if (data_register - change_value < 0)
				return;
			pwm->dat1 -= change_value;
		}
	}

	// If channel 2 is enabled
	if (pwm->ctl & RPI_PWM_CTL_PWEN2) {
		range = pwm->rng2;
		data_register = pwm->dat2;
		current_duty_cycle = (data_register * 100)/range;
		printf("Current duty cycle is %u\n", current_duty_cycle);
		
		uint32_t change_value = (percentage * range) / 100;
		if (change == INCREASE)
			pwm->dat2 += change_value;
		else if (change == DECREASE)
			pwm->dat2 -= change_value;
	}
}

static void ctrl_c_handler(int signum)
{
	(void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main(int argc, char *argv[])
{
	// Parse arguments function
	parse_args(argc, argv);

	// The below line has been added to view the transition from 0% to max and back to 0%
	duty_cycle = 0;

	int ret_status = 0;

	test_device_t *test_device = malloc(sizeof(test_device_t));

	// Pointer to store the hardware version and peripheral base
	test_device->rpi_hw =  rpi_hw_detect();

	ret_status = map_registers_pwm(test_device);
	if (ret_status == -1) {
		printf("Unable to map registers\n");
		exit(EXIT_FAILURE);
	}

	ret_status = check_pin_setup(test_device);
	if (ret_status == -1) {
		printf("Pin setup is incorrect\n");
		goto cleanup;
	}

	pwm_register_config(test_device);

	set_pwm_max(test_device, 100);

	pwm_set_duty_cycle(test_device);

	enable_pwm(test_device);

    while (running)
    {
		for (int i = 0; i < 10; i++) {
			pwm_duty_cycle_change(test_device, 10, INCREASE);
			usleep(200000);
		}

		for (int i = 0; i < 10; i++) {
			pwm_duty_cycle_change(test_device, 10, DECREASE);
			usleep(200000);
		}
    }

	disable_pwm(test_device);

	cleanup: unmap_registers_pwm(test_device);

    printf ("\n");
    return 0;
}
