#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <getopt.h>

using namespace std;

#define POWER_ON_PIN 154 //12 pin  = 131 GPIO number, 16 pin = 154 GPIO number
#define TEMPERATURE_FILE "/sys/class/thermal/thermal_zone0/temp"
#define PIPE_NAME "/tmp/fancontrol.pipe"


volatile sig_atomic_t flag = 1;

void sig_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        flag = 0;
    }
}

int readTemp(){
    std::fstream ftemp;
    std::string raw;

    ftemp.open(TEMPERATURE_FILE,std::ios::in);
    if (ftemp.is_open()){ 
        getline(ftemp, raw);
        ftemp.close(); 
        return std::stoi(raw)/1000;
    }else{
        std::cerr << "Read temperature error. Cannot open file.\n";
    }

    return -1;
}

static int daemonize(void)
{
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		return -1;
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		return -2;
	}

	/* Change the current working directory.  This prevents the current
	directory from being locked; hence not being able to remove it. */
	if ((chdir("/")) < 0) {
		return -3;
	}

	if (freopen( "/dev/null", "r", stdin) == NULL) {

	}
	if (freopen( "/dev/null", "r", stdout) == NULL) {

	}
	if (freopen( "/dev/null", "r", stderr) == NULL) {

	}

	return 0;
}

void help(void){
    std::cout << "FanControl\n";
    std::cout << "   Control fan connected to RockPi4 pin " << POWER_ON_PIN << " according to CPU temperature\n";
//    std::cout << "\n";
}

int main(int argc, char *argv[])
{
    int daemon=0, opt, long_index = 0;

    // install signal handler
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    // dont die if pipe closed 
    signal(SIGPIPE, SIG_IGN);

    static struct option long_options[] = {
	{"daemon", no_argument,	0, 'd'},
	{"help", no_argument,	0, 'h'}
    };

    while ((opt = getopt_long(argc, argv,"dh", long_options, &long_index )) != -1) { 
        switch (opt) {
            case 'd':
                daemon = 1;
                if(daemonize()<0){
	            return EXIT_FAILURE;
                }
                break;
            case 'h':
                help();
                return EXIT_SUCCESS;
        }
    }

    int _temp,_pipe_fd=-1;
    char _gpiopath[50];
    char _pipestr[80];
    int _fanRun = 0;

    std::cout << "Starting control fan with pin " << POWER_ON_PIN << "..." << std::endl;

    // initialize GPIO pin
    ofstream pinExport("/sys/class/gpio/export");
    pinExport << POWER_ON_PIN;
    if(!daemon)
        std::cout << "Pin " << POWER_ON_PIN << " exported" << std::endl;
    pinExport.close();

    sprintf(_gpiopath, "/sys/class/gpio/gpio%d/direction",POWER_ON_PIN); 

    ofstream pinDirection(_gpiopath);
    pinDirection << "out";
    pinDirection.close();
    if(!daemon)
        std::cout << "Pin " << POWER_ON_PIN << " set to output" << std::endl;

    sprintf(_gpiopath, "/sys/class/gpio/gpio%d/value",POWER_ON_PIN); 

    ofstream pinValue(_gpiopath);
    pinValue << "0";
    if(!daemon)
        std::cout << "Pin " << POWER_ON_PIN << " initialized to 0" << std::endl;

    // create fifo
    mkfifo(PIPE_NAME, 0644);
    //_pipe_fd = open(PIPE_NAME, O_WRONLY | O_NONBLOCK);

    while (flag) {
        // read temperature
        _temp = readTemp();
        if(_temp==-1){
	    return EXIT_FAILURE;
        }

	// turn on fan
	if(_temp>60){
            _fanRun = 1;
	// turn off fan
        }else if(_temp<50){
            _fanRun = 0;
        }

        // write pin value
        pinValue << _fanRun;
	pinValue.flush();

        // try to open pipe
        if(_pipe_fd < 0){
            _pipe_fd = open(PIPE_NAME, O_WRONLY | O_NONBLOCK);
        }

	if(!daemon || _pipe_fd > 0)
	    sprintf(_pipestr, "%d;%d\n",_fanRun, _temp); 

        // write into opened pipe
	if(_pipe_fd > 0){
            if(dprintf(_pipe_fd, _pipestr) < 0){
	        close(_pipe_fd);
                _pipe_fd = -1;
            }
        }

	if(!daemon){
            std::cout << "Current temp: " << _temp << "C -> "; 
            std::cout << _pipestr ;
	}

	sleep(10);
    }

    if(!daemon){
        std::cout << "Reset pin " << POWER_ON_PIN << " to 0" << std::endl;
	std::cout << "Bye bye" << std::endl;
    }

    pinValue << "0";
    pinValue.close();

    if(_pipe_fd > 0){
        close(_pipe_fd);
    }
    unlink(PIPE_NAME);

    return EXIT_FAILURE;
}

