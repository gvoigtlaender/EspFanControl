/* Copyright 2021 Georg Voigtlaender gvoigtlaender@googlemail.com */
#ifndef SRC_CFANCONTOL_H_
#define SRC_CFANCONTOL_H_

#include <Arduino.h>
#include <CBase.h>
#include <CControl.h>
#include <list>
#include <string>

#include <vector>
using std::vector;

class CSensorDS18B20;
class CMqttValue;
template <typename T> class CConfigKey;
class CXbm;

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

  uint8_t temp2Y(double dTemp);

  void OnButtonClick();
  void OnButtonLongClick();

  E_STATES m_eControlState;
  enum E_FANCONTROLMODE { eAutomatic = 0, eOn, eOff };
  E_FANCONTROLMODE m_eFanControlMode = eAutomatic;

  void SwitchControlMode(E_FANCONTROLMODE eMode);

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

  void updateStateString();

  bool m_bIsOn = false;
  int m_nPin = -1;
  CSensorDS18B20 *m_pDS18B20 = NULL;

  CXbm *m_pXBM_Temp = nullptr;

  class CValues {
  public:
    std::string m_sName;
    uint32_t m_Cycle = 1000;
    uint32_t m_millis = millis();
    vector<uint8_t> m_ValuesT;
    vector<bool> m_ValuesF;

    void push_back(uint8_t t, bool f, uint8_t mx) {
#ifdef _DEBUG
      CControl::Log(CControl::D, "%s->CValues::push_back(%u, %s, %u)",
                    m_sName.c_str(), t, f ? "true" : "false", mx);
#endif
      m_ValuesT.push_back(t);
      m_ValuesF.push_back(f);
      if (m_ValuesT.size() > mx) {
        m_ValuesT.erase(m_ValuesT.begin());
        m_ValuesF.erase(m_ValuesF.begin());
      }
    }
    uint8_t size() { return m_ValuesT.size(); }
  };
  enum E_VALUES { e5m = 0, e30m, e1h, e6h, e12h, e24h, eMax };
  CValues m_Values[E_VALUES::eMax];
  E_VALUES m_eValue = E_VALUES::e5m;
  CValues *m_ValuesAct = &m_Values[m_eValue];
  void UpdateXBM(bool bForce);
};

#endif // SRC_CFANCONTOL_H_