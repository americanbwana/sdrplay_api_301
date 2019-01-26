#include <iostream>
#include <windows.h>
#include <cctype>
#include <conio.h>
#include "sdrplay_api.h"
int masterInitialised = 0;
int slaveUninitialised = 0;

sdrplay_api_DeviceT *chosenDevice = NULL;

using namespace std;

// functions to replace the labels
void CloseAPI() {
     sdrplay_api_Close();   
}

void UnlockDeviceAndCloseApi() {
    sdrplay_api_UnlockDeviceApi();
    CloseAPI();
}

//  to control db gain using keyboard 
const short unsigned int Keyleft  = 37;
const short unsigned int Keytop   = 38;
const short unsigned int Keyright = 39;
const short unsigned int Keydown  = 40;
const short unsigned int Keyexit  = 81;

int getKey() {
	while (true)
	{
		for(int i = 8; i <= 256; i++)
		{
			if(GetAsyncKeyState(i) & 0x7FFF)
			{
				
				// This if filters the keys, i want to allow direction arrows
				// and q for quit. If you want to add more just add the code for the key,
				// to know the key code just coment the if line and print the keycode. 
				if( ( i >= 37 && i <= 40 ) || i == 81 )
				return i;
			}
		}
	}
}



void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        cout << "sdrplay_api_StreamACallback: numSamples=" << numSamples << endl;

    // Process stream callback data here

    return;
}

void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        cout << "sdrplay_api_StreamBCallback: numSamples=" << numSamples << endl;

    // Process stream callback data here - this callback will only be used in dual tuner mode

    return;
}

void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
    switch(eventId)
    {
    case sdrplay_api_GainChange:
        printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n", "sdrplay_api_GainChange", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B", params->gainParams.gRdB, params->gainParams.lnaGRdB, params->gainParams.currGain);
        break;

    case sdrplay_api_PowerOverloadChange:
        cout <<("sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B", (params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected)? "sdrplay_api_Overload_Detected": "sdrplay_api_Overload_Corrected");
        // Send update message to acknowledge power overload message received
        sdrplay_api_Update(chosenDevice->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck);
        break;

    case sdrplay_api_RspDuoModeChange:
        printf("sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n", "sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B", (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)? "sdrplay_api_MasterInitialised": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)? "sdrplay_api_SlaveAttached": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)? "sdrplay_api_SlaveDetached": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)? "sdrplay_api_SlaveInitialised": 
                                                                                                                                                                                    (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)? "sdrplay_api_SlaveUninitialised": 
                                                                                                                                                                                    "unknown type");
        if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
        {
            masterInitialised = 1;
        }
        if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
        {
            slaveUninitialised = 1;
        }
        break;

    case sdrplay_api_DeviceRemoved:
        cout << ("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
        break;

    default:
        cout << "sdrplay_api_EventCb: " << eventId << " unknown event\n";
        break;
    }
}

void usage(void)
{
    cout <<("Usage: sample_app.exe [A|B] [ms]\n");
    exit(1);
}
int main(int argc, char *argv[]) {
    sdrplay_api_DeviceT devs[6];
    unsigned int ndev;
    float ver = 0.0;
    sdrplay_api_ErrT err;
//    err = sdrplay_api_ApiVersion(&ver);
    unsigned int i;

//    sdrplay_api_DeviceT devices[4];
    sdrplay_api_DeviceParamsT *deviceParams = NULL;
    sdrplay_api_CallbackFnsT cbFns;
    sdrplay_api_RxChannelParamsT *chParams;

    int reqTuner = 0;
    int master_slave = 0;

    unsigned int chosenIdx = 0;
        if ((argc > 1) && (argc < 4))
    {
        if (!strcmp(argv[1], "A"))
        {
            reqTuner = 0;
        }
        else if (!strcmp(argv[1], "B"))
        {
            reqTuner = 1;
        }
        else
        {
            usage();
        }
        if (argc == 3)
        {
            if (!strcmp(argv[2], "ms"))
            {
                master_slave = 1;
            }
            else
            {
                usage();
            }
        }
    }
    else if (argc >= 4)
    {
        usage();
    }

    cout <<("requested Tuner%c Mode=%s\n", (reqTuner == 0)? 'A': 'B', (master_slave == 0)? "Single_Tuner": "Master/Slave");
    
        // Open API
    if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
    {
        cout <<"sdrplay_api_Open failed " << sdrplay_api_GetErrorString(err);
    }
        // Enable debug logging output
        if ((err = sdrplay_api_DebugEnable(NULL, 1)) != sdrplay_api_Success)
        {
            cout << "sdrplay_api_DebugEnable failed " << sdrplay_api_GetErrorString(err);
        }
        // Check API versions match
        if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success)
        {
            cout <<"sdrplay_api_ApiVersion failed " << sdrplay_api_GetErrorString(err);
        } 
        if (ver != SDRPLAY_API_VERSION)
        {
            cout <<"API version don't match (local=" << SDRPLAY_API_VERSION << " dll=" <<  ver << endl;
            CloseAPI();
        }
    std::cout << "API Version = " << ver << std::endl;

    // 
    // Lock API while device selection is performed
    sdrplay_api_LockDeviceApi();
    
            // Fetch list of available devices
        if ((err = sdrplay_api_GetDevices(devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success)
        {
            cout << "sdrplay_api_GetDevices failed " << sdrplay_api_GetErrorString(err);
            UnlockDeviceAndCloseApi();
        }
        
        cout << "MaxDevs=" << (sizeof(devs) / sizeof(sdrplay_api_DeviceT)) << "%d NumDevs=" << ndev;
        if (ndev > 0)
        {
            for (i = 0; i < (unsigned int)ndev; i++)
            {
                if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
                    cout <<("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i, devs[i].SerNo, devs[i].hwVer , devs[i].tuner, devs[i].rspDuoMode);
                else
                    cout <<("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, devs[i].SerNo, devs[i].hwVer, devs[i].tuner);
            }
        }
        
                    // Choose device
            if ((reqTuner == 1) || (master_slave == 1))  // requires RSPduo
            {
                // Pick first RSPduo
                for (i = 0; i < (unsigned int)ndev; i++)
                {
                    if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
                    {
                        chosenIdx = i;
                        break;
                    }
                }
            }
            else
            {
                // Pick first device of any type
                for (i = 0; i < (unsigned int)ndev; i++)
                {
                    chosenIdx = i;
                    break;
                }
            }
            if (i == ndev)
            {
            cout <<("Couldn't find a suitable device to open - exiting\n");
                UnlockDeviceAndCloseApi();
            }
            cout << "chosenDevice = " << chosenIdx;
            chosenDevice = &devs[chosenIdx];
  
            // Select chosen device
            if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success)
            {
                cout << "sdrplay_api_SelectDevice failed " <<  sdrplay_api_GetErrorString(err);
                UnlockDeviceAndCloseApi();
            }

            // Unlock API now that device is selected
            sdrplay_api_UnlockDeviceApi();

            // Retrieve device parameters so they can be changed if wanted
            if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success)
            {
                cout << "sdrplay_api_GetDeviceParams failed " <<sdrplay_api_GetErrorString(err) << endl;
                CloseAPI();
            }

            // Check for NULL pointers before changing settings
            if (deviceParams == NULL)
            {
                cout <<("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
                CloseAPI();
            }          
            
            // Configure dev parameters
            if (deviceParams->devParams != NULL) // This will be NULL for slave devices as only the master can change these parameters
            {
                // Only need to update non-default settings
                if (master_slave == 0)
                {
                    // Change from default Fs  to 8MHz
                    deviceParams->devParams->fsFreq.fsHz = 8000000.0;
                }
                else 
                {
                    // Can't change Fs in master/slave mode
                }
            }
            
            
            // Configure tuner parameters (depends on selected Tuner which set of parameters to use)
            chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B)? deviceParams->rxChannelB: deviceParams->rxChannelA;
            if (chParams != NULL)
            {
                chParams->tunerParams.rfFreq.rfHz = 220000000.0;
                chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
                if (master_slave == 0) // Change single tuner mode to ZIF
                {
                    chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
                }
                chParams->tunerParams.gain.gRdB = 40;
                chParams->tunerParams.gain.LNAstate = 5;

                // Disable AGC
                chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
            }
            else
            {
                cout << "sdrplay_api_GetDeviceParams returned NULL chParams pointer" << endl;
                CloseAPI();
            }            

            // Assign callback functions to be passed to sdrplay_api_Init()
            cbFns.StreamACbFn = StreamACallback;
            cbFns.StreamBCbFn = StreamBCallback;
            cbFns.EventCbFn = EventCallback;
            
                        // Now we're ready to start by calling the initialisation function
            // This will configure the device and start streaming
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
            {
                cout << "sdrplay_api_Init failed " << sdrplay_api_GetErrorString(err) << endl;
                if (err == sdrplay_api_StartPending) // This can happen if we're starting in master/slave mode as a slave and the master is not yet running
                {
                    while(1)
                    {
                        Sleep(1000);
                        if (masterInitialised) // Keep polling flag set in event callback until the master is initialised
                        {
                            // Redo call - should succeed this time
                            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
                            {
                                cout << "sdrplay_api_Init failed " <<  sdrplay_api_GetErrorString(err) << endl;
                            }
                            CloseAPI();
                        }
                        cout <<("Waiting for master to initialise\n");
                    }
                }
                else
                {
                    CloseAPI();
                }
            }
 // Small keyboard control loop to control gain in +/- 1db using keyboard keys
	int key;
	cout << "+ = Up Arrow, - = Down Arrow, q = q \n" << endl;

	while (true) 
	{
		key = getKey();
		

		switch( key )
		{

//		case Keyleft:
//			{
//				cout << "you pressed : left" <<endl;
//			}
//			continue;
		case Keytop:
			{
                cout <<  "Current gRdb is " << chParams->tunerParams.gain.gRdB << endl;
				cout << "Increasing gain.  New value is " << (chParams->tunerParams.gain.gRdB) + 1 << endl ; // Assuming debug is on

                        // Limit it to a maximum of 59dB
                        if (chParams->tunerParams.gain.gRdB > 59)
                            chParams->tunerParams.gain.gRdB = 20;
                        else chParams->tunerParams.gain.gRdB += 1;
                        
                        if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner, sdrplay_api_Update_Tuner_Gr)) != sdrplay_api_Success)
                        {
                            cout << "sdrplay_api_Update sdrplay_api_Update_Tuner_Gr failed " <<  sdrplay_api_GetErrorString(err) << endl;
                            break;
                        }

			} 
            // chParams->tunerParams.gain.gRdB += 1;
			continue;
//		case Keyright:
//			{
//				cout << "you pressed : right" <<endl;
//			}
//			continue;
		case Keydown:
			{
				cout <<  "Current gRdb is " << chParams->tunerParams.gain.gRdB << endl;
                cout << "Reducing gain.  New value is "<< (chParams->tunerParams.gain.gRdB) + 1 << endl ;
  //               {
                        chParams->tunerParams.gain.gRdB -= 1;
                        // Limit it to a minimum of 20dB
                        if (chParams->tunerParams.gain.gRdB < 20)
                            chParams->tunerParams.gain.gRdB = 59;
                        else chParams->tunerParams.gain.gRdB += 1;    
                        if ((err = sdrplay_api_Update(chosenDevice->dev, chosenDevice->tuner, sdrplay_api_Update_Tuner_Gr)) != sdrplay_api_Success)
                        {
                            cout << "sdrplay_api_Update sdrplay_api_Update_Tuner_Gr failed " << sdrplay_api_GetErrorString(err) << endl;
                            break;
                        }
//                    }
			}
			continue;
		case Keyexit:
			{
				cout << "exit key" <<endl;
			}
            return 0;
// 			break;
		default:
			{
				cout << "you pressed : " << i <<endl;
		};
	}
	
// 	system ("pause");
// 	return 0;
}
 
          
            // Finished with device so uninitialise it
            if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
            {
                cout << "sdrplay_api_Uninit failed " <<  sdrplay_api_GetErrorString(err) << endl;
                if (err == sdrplay_api_StopPending) // This can happen if we're stopping in master/slave mode as a master and the slave is still running
                {
                    while(1)
                    {
                        Sleep(1000);
                        if (slaveUninitialised) // Keep polling flag set in event callback until the slave is uninitialised
                        {
                            // Repeat call - should succeed this time
                            if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
                            {
                                cout <<"sdrplay_api_Uninit failed " <<  sdrplay_api_GetErrorString(err) << endl;
                            }
                            slaveUninitialised = 0;
                            CloseAPI();
                        }
                        cout <<("Waiting for slave to uninitialise\n");
                    }
                }
                CloseAPI();
            }
            
        // Release device (make it available to other applications)
        sdrplay_api_ReleaseDevice(chosenDevice);
    // unlock and close the API
    // sdrplay_api_UnlockDeviceApi();
    sdrplay_api_Close();

    return 0;
}