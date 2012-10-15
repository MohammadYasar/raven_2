/**
*  File: console_process.cpp
*
*  Outputs data to the console periodically, so that we know our robot is alives.
*/

#include <stdio.h>
#include <iomanip>
#include <termios.h>   // needed for terminal settings in getkey()
#include <sstream>

#include "rt_process_preempt.h"
#include "rt_raven.h"
#include "shared_modes.h"

#include <raven/util/timing.h>

#include <raven/state/runlevel.h>
#include <raven/state/device.h>
#include <raven/control/control_input.h>
using namespace std;

// from rt_process.cpp
extern struct device device0;

extern unsigned long int gTime;
extern int soft_estopped;
extern struct DOF_type DOF_types[];
extern int initialized;

void outputRobotState();
void outputTiming();
int getkey();

/**
* This is the thread dedicated to console io
*/
void *console_process(void *)
{
    ros::Time t1, t2;
    ros::Duration d;
    t1=t1.now();
    t2=t2.now();

    //Low priority non realtime thread
    struct sched_param param;                    // priority settings
    param.sched_priority = 0;
    if (sched_setscheduler(0, SCHED_OTHER, &param)==-1)
    {
        perror("sched_setscheduler failed for console process");
        exit(-1);
    }

    int output_robot=false;
    bool output_timing=false;
    int theKey,print_msg=1;
    bool masterModeWasNone = true;
    char inputbuffer[100];
    sleep(1);
    // Run shell interaction
    while (ros::ok())
    {
    	MasterMode masterMode;
    	std::set<MasterMode> masterModeConflicts;
    	getMasterModeConflicts(masterMode,masterModeConflicts);
    	if (!masterModeConflicts.empty() || (masterModeWasNone && masterMode != MasterMode::NONE)) {
    		print_msg = 1;
    	}
    	masterModeWasNone = (masterMode == MasterMode::NONE);

    	t_controlmode controlMode;
		std::set<t_controlmode> controlModeConflicts;
		getControlModeConflicts(controlMode,controlModeConflicts);
		if (!controlModeConflicts.empty()) {
			print_msg = 1;
		}

        // Output UI hints
        if ( print_msg ){

        	if (!masterModeConflicts.empty()) {
        		std::stringstream ss;
				ss << "Master mode conflicts from";
				for (std::set<MasterMode>::iterator itr=masterModeConflicts.begin();itr!=masterModeConflicts.end();itr++) {
					ss << " " << itr->str();
				}
				ss << ", current mode is " << masterMode.str();
				log_warn(ss.str().c_str());
        	} else if (masterMode != MasterMode::NONE) {
        		log_msg("In master mode %s",masterMode.str());
        	}

        	if (!controlModeConflicts.empty()) {
				std::stringstream ss;
				ss << "Control mode conflicts from ";
				for (std::set<t_controlmode>::iterator itr=controlModeConflicts.begin();itr!=controlModeConflicts.end();itr++) {
					ss << " " << controlModeToString(*itr);
				}
				ss << ", current mode is " << controlModeToString(controlMode);
				log_warn(ss.str().c_str());
			}

            log_msg("[[\t'C'  : toggle console messages ]]");

            if (masterMode != MasterMode::NONE || !masterModeConflicts.empty()) {
			log_msg("[[\t'U'  : unlock master mode      ]]");
            }

            log_msg("[[\t'T'  : specify joint torque    ]]");
            log_msg("[[\t'M'  : set control mode        ]]");
            log_msg("[[\t'^C' : Quit                    ]]");
            print_msg=0;
        }
        // Get UI command
        theKey = getkey();
        switch (theKey){
            case 'z':
            {
                output_robot = 0;
                setDofTorque(0,0,0);
                log_msg("Torque zero'd");
                print_msg=1;
                break;
            }

            case 'e':
            case 'E':
            case '0':
            {
                output_robot = 0;
#ifdef USE_NEW_RUNLEVEL
                RunLevel::eStop();
#else
                soft_estopped=TRUE;
#endif
                print_msg=1;
                log_msg("Soft estopped");
                break;
            }
            case '+':
            case '=':
            {
                soft_estopped=FALSE;
                print_msg=1;
                log_msg("Soft estop off");
                break;
            }
            case 'c':
            case 'C':
            {
            	output_robot = !output_robot;
            	log_msg("Console output: %s", output_robot?"on":"off");
                print_msg=1;
                break;
            }
            case 't':
            case 'T':
            {
                print_msg=1;
                // Get user-input mechanism #
                printf("\n\nEnter a mechanism number: 0-Gold, 1-Green:\t");
                cin.getline (inputbuffer,100);
                unsigned int _mech = atoi(inputbuffer);
                if ( _mech > 1 ) break;

                // Get user-input joint #
                printf("\nEnter a joint number: 0-shoulder, 1-elbow, 2-zins, 4-tool_rot, 5-wrist, 6/7- grasp 1/2:\t");
                cin.getline (inputbuffer,100);
                unsigned int _joint = atoi(inputbuffer);
                if ( _joint > MAX_DOF_PER_MECH ) break;

                // Get user-input DAC value #
                printf("\nEnter a torque value in miliNewton-meters:\t");
                cin.getline (inputbuffer,100);
                int _torqueval = atoi(inputbuffer);

                log_msg("Commanded mech.joint (tau):%d.%d (%d))\n",_mech, _joint, _torqueval);
                setDofTorque(_mech, _joint, _torqueval);
                break;
            }
            case 'm':
            case 'M':
            {
                // Get user-input DAC value #
                printf("\n\nEnter new control mode: 0=NULL, 1=NULL, 2=joint_velocity, 3=apply_torque, 4=homing, 5=motor_pd, 6=cartesian_space_motion, 7=multi_dof_sinusoid 8=joint_position \t");
                cin.getline (inputbuffer,100);
                t_controlmode _cmode = (t_controlmode)(atoi(inputbuffer));
                log_msg("received control mode:%d\n\n",_cmode);
                setRobotControlMode(_cmode);
                print_msg=1;
                break;
            }
            case 'u':
            case 'U':
            {
            	print_msg=1;
            	log_msg("Unlocking master masterMode");
            	bool resetResult = resetMasterMode();
            	if (!resetResult) {
            		log_err("Could not unlock master masterMode!");
            	}
            	break;
            }
            case 'v':
            case 'V':
            {
            	output_timing = !output_timing;
            	log_msg("Console timing output: %s", output_timing? "on" : "off");
            	print_msg=1;
            	break;
            }
        }

        // Output the robot state once/sec
        if ( (output_robot || output_timing)
        		&& (t1.now()-t1).toSec() > 1 ) {
        	if (output_robot) {
        		outputRobotState();
        	}
            if (output_timing) {
            	outputTiming();
            }
            t1=t1.now();
        }

//        printf("g: %1.5f %1.5f\n",device0.mech[0].joint[GRASP1].jpos,device0.mech[1].joint[GRASP1].jpos);
//        printf("d: %1.5f %1.5f\n\n",device0.mech[0].joint[GRASP1].jpos_d,device0.mech[1].joint[GRASP1].jpos_d);

        usleep(1e5); //Sleep for 1/10 second
    }

    return(NULL);
}

int getkey() {
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}

void outputRobotState(){
	RunLevel rl = RunLevel::get();
	cout << "Runlevel: " << RunLevel::get().str() << " " << rl.isInit() << " " << rl.isInitSublevel(0) << " " << rl.isInitSublevel(1) << " " << rl.isInitSublevel(2) << endl;
	cout << "Pedal: " << RunLevel::getPedal() << " " << device0.surgeon_mode << endl;
	cout << "inited " << RunLevel::isInitialized() << " " << initialized << endl;
	cout << "Runlevel: " << static_cast<unsigned short int>(device0.runlevel) << endl;
    cout << "Master mode: " << getMasterMode().str() << endl;
    cout << "Controller: " << controlModeToString(getControlMode()) << endl;
    mechanism* _mech = NULL;
    int mechnum=0;
#ifdef USE_NEW_DEVICE
    DevicePtr dev = Device::current();
    OldControlInputPtr input = ControlInput::getOldControlInput();
#endif
    while (loop_over_mechs(&device0,_mech,mechnum)) {
#ifdef USE_NEW_DEVICE
    	ArmPtr arm = dev->getArmById(_mech->type);
#endif
        if (_mech->type == GOLD_ARM)
            cout << "Gold arm:\t";
        else if (_mech->type == GREEN_ARM)
            cout << "Green arm:\t";
        else
            cout << "Unknown arm:\t";

        cout << "Board " << mechnum << ", type " << _mech->type << ":" << endl;
//
//        cout<<"pos:\t";
//        cout<<_mech->pos.x<<"\t";
//        cout<<_mech->pos.y<<"\t";
//        cout<<_mech->pos.z<<"\n";
//
//        cout<<"pos_d:\t";
//        cout<<_mech->pos_d.x<<"\t";
//        cout<<_mech->pos_d.y<<"\t";
//        cout<<_mech->pos_d.z<<"\n";

        DOF* _joint = NULL;
        int jnum=0;

        cout<<"type:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << _joint->type<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"     \t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << std::string(arm->getJointByOldType(_joint->type)->type().str()).substr(0,6) <<"\t";
        cout << endl;
#endif

        cout<<"enc_val:\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << _joint->enc_val<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getMotorByOldType(_joint->type)->encoderValue()<<"\t";
        cout << endl;
#endif

        cout<<"enc_off:\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << _joint->enc_offset<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getMotorByOldType(_joint->type)->encoderOffset()<<"\t";
        cout << endl;
#endif

        cout<<"mpos:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(2)<<_joint->mpos <<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getMotorByOldType(_joint->type)->position()<<"\t";
        cout << endl;
#endif

        cout<<"mpos_d:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(2)<<_joint->mpos_d <<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << input->motorPositionByOldType(_joint->type) <<"\t";
        cout << endl;
#endif

        cout<<"mvel:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(0)<<_joint->mvel <<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getMotorByOldType(_joint->type)->velocity()<<"\t";
        cout << endl;
#endif

        cout<<"mvel_d:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(0)<<_joint->mvel_d <<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << input->motorVelocityByOldType(_joint->type) <<"\t";
        cout << endl;
#endif

        cout<<"jpos:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	if (jnum != Z_INS)
        		cout<<fixed<<setprecision(2)<<_joint->jpos <<"\t";
        	else
        		cout<<fixed<<setprecision(2)<<_joint->jpos<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getJointByOldType(_joint->type)->position()<<"\t";
        cout << endl;
#endif

        cout<<"jpos_d:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	if (jnum != Z_INS)
        		cout<<fixed<<setprecision(2)<<_joint->jpos_d <<"\t";
        	else
        		cout<<fixed<<setprecision(2)<<_joint->jpos_d<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << input->jointPositionByOldType(_joint->type) <<"\t";
        cout << endl;
#endif

        cout<<"jvel:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	if (jnum != Z_INS)
        		cout<<fixed<<setprecision(2)<<_joint->jvel <<"\t";
        	else
        		cout<<fixed<<setprecision(2)<<_joint->jvel<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getJointByOldType(_joint->type)->velocity()<<"\t";
        cout << endl;
#endif

        cout<<"jvel_d:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	if (jnum != Z_INS)
        		cout<<fixed<<setprecision(2)<<_joint->jvel_d <<"\t";
        	else
        		cout<<fixed<<setprecision(2)<<_joint->jvel_d<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << input->jointVelocityByOldType(_joint->type) <<"\t";
        cout << endl;
#endif

        cout<<"tau_d:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(3)<<_joint->tau_d<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << input->motorTorqueByOldType(_joint->type) <<"\t";
        cout << endl;
#endif

        cout<<"DAC:\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(3)<<_joint->current_cmd<<"\t";
        cout << endl;

#ifdef USE_NEW_DEVICE
        cout<<"        \t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << arm->getMotorByOldType(_joint->type)->dacCommand()<<"\t";
        cout << endl;
#endif

        cout<<"KP gains:\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(3)<<DOF_types[mechnum*MAX_DOF_PER_MECH+jnum].KP<<"\t";
        cout << endl;

        cout<<"KD gains:\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
            cout<<fixed<<setprecision(3)<<DOF_types[mechnum*MAX_DOF_PER_MECH+jnum].KD<<"\t";
        cout << endl;

        cout<<"KI gains:\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout<<fixed<<setprecision(3)<<DOF_types[mechnum*MAX_DOF_PER_MECH+jnum].KI<<"\t";
        cout << endl;

//        cout<<"enc_offset:\t";
//        _joint = NULL; jnum=0;
//        while (loop_over_joints(_mech,_joint,jnum))
//            cout<<_joint->enc_offset<<"\t";
//        cout << endl;


        cout << endl;
    }

    /*
    std::stringstream ss;
    ss << std::endl;
	FOREACH_ARM_IN_DEVICE(arm1,Device::current()) {
		ss << " arm " << arm1->name() << std::endl;
		FOREACH_MOTOR_IN_ARM(motor,arm1) {
			//ss << " " << motor->encoderValue();
			ss << motor->str() << std::endl;
		}
		FOREACH_JOINT_IN_ARM(joint,arm1) {
			//ss << " " << motor->encoderValue();
			ss << joint->str() << std::endl;
		}
		//ss << " " << std::endl;
	}
	std::cout << ss.str();


	std::cout << "grasp " << Controller::getControlInput<OldControlInput>(std::string("old"))->arm(0).grasp() << std::endl;
*/

    /*
    cout << "******************DIFFS**********************" << endl;

    _mech = NULL;
    while (loop_over_mechs(&device0,_mech,mechnum)) {
    	ArmPtr arm = dev->getArmById(_mech->type);

    	if (_mech->type == GOLD_ARM)
    		cout << "Gold arm:\t";
    	else if (_mech->type == GREEN_ARM)
    		cout << "Green arm:\t";
    	else
    		cout << "Unknown arm:\t";

    	cout << "Board " << mechnum << ", type " << _mech->type << ":" << endl;
//
//        cout<<"pos:\t";
//        cout<<_mech->pos.x<<"\t";
//        cout<<_mech->pos.y<<"\t";
//        cout<<_mech->pos.z<<"\n";
//
//        cout<<"pos_d:\t";
//        cout<<_mech->pos_d.x<<"\t";
//        cout<<_mech->pos_d.y<<"\t";
//        cout<<_mech->pos_d.z<<"\n";

    	DOF* _joint = NULL;
    	int jnum=0;

    	cout<<"type:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout << _joint->type<<"\t";
    	}
    	cout << endl;

        cout<<"    :\t\t";
        _joint = NULL; jnum=0;
        while (loop_over_joints(_mech,_joint,jnum))
        	cout << std::string(arm->getJointByOldType(_joint->type)->type().str()).substr(0,6) <<"\t";
        cout << endl;

    	cout<<"enc_val:\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		if (arm->isGreen()) {
    			cout << arm->getMotorByOldType(_joint->type)->encoderValue() - _joint->enc_val<<"\t";
    		} else {
    			cout << arm->getMotorByOldType(_joint->type)->encoderValue() + _joint->enc_val<<"\t";
    		}
    	}
    	cout << endl;

    	cout<<"enc_off:\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout << arm->getMotorByOldType(_joint->type)->encoderOffset() - _joint->enc_offset<<"\t";
    	}
    	cout << endl;

    	cout<<"mpos:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout<<fixed<<setprecision(2)<<arm->getMotorByOldType(_joint->type)->position() - _joint->mpos <<"\t";
    	}
    	cout << endl;

    	cout<<"mpos_d:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout<<fixed<<setprecision(2)<< input->motorPositionByOldType(_joint->type) - _joint->mpos_d <<"\t";
    	}
    	cout << endl;

    	cout<<"mvel:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout<<fixed<<setprecision(0)<<arm->getMotorByOldType(_joint->type)->velocity() - _joint->mvel <<"\t";
    	}
    	cout << endl;

    	cout<<"mvel_d:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout<<fixed<<setprecision(0)<<input->motorVelocityByOldType(_joint->type) - _joint->mvel_d <<"\t";
    	}
    	cout << endl;

    	cout<<"jpos:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		if (jnum != Z_INS)
    			cout<<fixed<<setprecision(2)<<arm->getJointByOldType(_joint->type)->position() - _joint->jpos <<"\t";
    		else
    			cout<<fixed<<setprecision(2)<<arm->getJointByOldType(_joint->type)->position() - _joint->jpos<<"\t";
    	}
    	cout << endl;

    	cout<<"jpos_d:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		if (jnum != Z_INS)
    			cout<<fixed<<setprecision(2)<<input->jointPositionByOldType(_joint->type) - _joint->jpos_d <<"\t";
    		else
    			cout<<fixed<<setprecision(2)<<input->jointPositionByOldType(_joint->type) - _joint->jpos_d<<"\t";
    	}
    	cout << endl;

    	cout<<"jvel:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		if (jnum != Z_INS)
    			cout<<fixed<<setprecision(2)<<arm->getJointByOldType(_joint->type)->velocity() - _joint->jvel <<"\t";
    		else
    			cout<<fixed<<setprecision(2)<<arm->getJointByOldType(_joint->type)->velocity() - _joint->jvel<<"\t";
    	}
    	cout << endl;

    	cout<<"jvel_d:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		if (jnum != Z_INS)
    			cout<<fixed<<setprecision(2)<<input->jointVelocityByOldType(_joint->type) - _joint->jvel_d <<"\t";
    		else
    			cout<<fixed<<setprecision(2)<<input->jointVelocityByOldType(_joint->type) - _joint->jvel_d<<"\t";
    	}
    	cout << endl;

    	cout<<"tau_d:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout<<fixed<<setprecision(3)<<input->motorTorqueByOldType(_joint->type) - _joint->tau_d<<"\t";
    	}
    	cout << endl;

    	cout<<"DAC:\t\t";
    	_joint = NULL; jnum=0;
    	while (loop_over_joints(_mech,_joint,jnum)) {
    		if (jnum == 3) continue;
    		cout<<fixed<<setprecision(3)<<arm->getMotorByOldType(_joint->type)->dacCommand() - _joint->current_cmd<<"\t";
    	}
    	cout << endl;

//        cout<<"enc_offset:\t";
//        _joint = NULL; jnum=0;
//        while (loop_over_joints(_mech,_joint,jnum))
//            cout<<_joint->enc_offset<<"\t";
//        cout << endl;


    	cout << endl;
    }
    */
}

void outputTiming() {
	cout << TimingInfo::usb_read_stats() << endl;
	cout << TimingInfo::state_machine_stats() << endl;
	cout << TimingInfo::update_state_stats() << endl;
	cout << TimingInfo::control_stats() << endl;
	cout << TimingInfo::usb_write_stats() << endl;
	cout << TimingInfo::ros_stats() << endl;

	cout << TimingInfo::overall_stats() << endl;

	cout << USBTimingInfo::get_packet_stats() << endl;
	cout << USBTimingInfo::process_packet_stats() << endl;

	cout << ControlTiming::overall_stats() << endl;

	cout << endl;

	TimingInfo::reset();
	USBTimingInfo::reset();
}

