/* Copyright 2021 Georg Voigtlaender gvoigtlaender@googlemail.com */
#ifndef SRC_CFANCONTOL_H_
#define SRC_CFANCONTOL_H_

#include <Arduino.h>
#include <CBase.h>
#include <CConfigValue.h>
#include <CControl.h>
#include <list>

class CSensorDS18B20;
class CFanControl : public CControl {

public:
  CFanControl(int nPin);

  bool setup() override;

//! task control
#if CLight_CTRL_VER == 1
  void control(bool bForce /*= false*/) override;
  enum E_STATES{eInit = 0,        eWaitForNtp,    eCheck,
                eWaitForBlueOn,   eBlueOn,        eWaitForWhiteOn,
                eWhiteOn,         eWhiteOnCheck,  eWaitForNoonOff,
                eNoonOff,         eWaitForNoonOn, eNoonOn,
                eWaitForWhiteOff, eWhiteOff,      eWaitForBlueOff,
                eBlueOff,         eWaitForNewDay};
#else
  void control(bool bForce /*= false*/) override;
  enum E_STATES {
    eInit = 0,
    eControl,
  };
#endif

  void OnButtonClick();

  E_STATES m_eControlState;
  enum E_LIGHTCONTROLMODE { eAutomatic = 0, eOn, eOff };
  E_LIGHTCONTROLMODE m_eFanControlMode = eAutomatic;

  void SwitchControlMode(E_LIGHTCONTROLMODE eMode);

  CConfigKey<double> *m_pTempFanOn = NULL;
  CConfigKey<double> *m_pTempFanOff = NULL;

  CMqttValue *m_pMqtt_ControlMode = NULL;
  CMqttValue *m_pMqtt_FanState = NULL;
  CMqttValue *m_pMqtt_FanStateB = NULL;
  CMqttValue *m_pMqtt_Temperature = NULL;

  CMqttCmd *m_pMqtt_CmdSwitch = NULL;
  void ControlMqttCmdCallback(CMqttCmd *pCmd, byte *payload,
                              unsigned int length) override;

  void SetOutput(bool bOn);

  bool m_bIsOn = false;
  int m_nPin = -1;
  CSensorDS18B20 *m_pDS18B20 = NULL;
};

#endif // SRC_CFANCONTOL_H_