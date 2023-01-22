#include <CFanControl.h>
#include <CLed.h>
#include <CSensor.h>
#include <CWifi.h>
#include <config.h>
#include <ctime>

CFanControl::CFanControl(int nPin)
    : CControl("CFanCtrl"), m_eControlState(eInit),
      m_eFanControlMode(eAutomatic), m_nPin(nPin) {

  pinMode(nPin, OUTPUT);
  digitalWrite(nPin, LOW);

  m_pTempFanOn = CreateConfigKey<int>("FanControl", "TempFanOn", 25);

  m_pTempFanOff = CreateConfigKey<int>("FanControl", "TempFanOff", 23);
}

bool CFanControl::setup() {

  m_pMqtt_ControlMode = CreateMqttValue("ControlMode", "Automatic");
  m_pMqtt_FanState = CreateMqttValue("FanState", "Unknown");

  m_pMqtt_CmdSwitch = CreateMqttCmd("CmdSwitch");

  SwitchControlMode(eAutomatic);

  return true;
}

//! task control
void CFanControl::control(bool bForce /*= false*/) {

  // static unsigned long ulMillis = millis();

  if (m_eFanControlMode != eAutomatic) {
    const int nManualDelay = 2000;
    if (millis() > m_uiTime + nManualDelay) {
      CLed::AddBlinkTask(CLed::BLINK_2);
      m_uiTime += nManualDelay;
    }
    return;
  }

  if (m_uiTime > millis())
    return;

  switch (m_eControlState) {
  case eInit:
    m_uiTime = millis();
    _log2(I, "eInit");

    m_eControlState = eControl;
    break;

  case eControl:

    if (m_eFanControlMode == eAutomatic && m_pDS18B20 != NULL) {
      double dTemp = m_pDS18B20->GetTemperature(0);
      if (m_bIsOn) {
        if (dTemp < m_pTempFanOff->GetValue()) {
          SetOutput(false);
          _log(I, "FanOff for Temperature %.1f", dTemp);
        }
      } else {
        if (dTemp > m_pTempFanOn->GetValue()) {
          SetOutput(true);
          _log(I, "FanOn for Temperature %.1f", dTemp);
        }
      }
    }
    break;
  }
  m_uiTime += 100;

  static uint8 uiModulo = 0;
  if (++uiModulo % 100 == 0)
    CLed::AddBlinkTask(CLed::BLINK_1);
}

void CFanControl::OnButtonClick() {
  switch (m_eFanControlMode) {
  case eAutomatic:
    SwitchControlMode(eOn);
    break;

  case eOn:
    SwitchControlMode(eOff);
    break;

  case eOff:
    SwitchControlMode(eAutomatic);
    break;

  default:
    break;
  }
}

void CFanControl::SwitchControlMode(E_LIGHTCONTROLMODE eMode) {
  switch (eMode) {
  case eAutomatic:
    m_pMqtt_ControlMode->setValue("auto");
    SetOutput(m_bIsOn);
    _log2(CControl::I, "ControlMode=automatic");
    break;
  case eOn:
    m_pMqtt_ControlMode->setValue("on");
    _log2(CControl::I, "ControlMode=on");
    SetOutput(true);
    break;
  case eOff:
    m_pMqtt_ControlMode->setValue("off");
    _log2(CControl::I, "ControlMode=off");
    SetOutput(false);
    break;
  }
  m_eFanControlMode = eMode;
  CLed::AddBlinkTask(CLed::BLINK_1);
}

void CFanControl::ControlMqttCmdCallback(CMqttCmd *pCmd, byte *payload,
                                         unsigned int length) {
  CControl::ControlMqttCmdCallback(pCmd, payload, length);

  if (pCmd == m_pMqtt_CmdSwitch) {
    char szCmd[length + 1];
    strncpy(szCmd, (const char *)payload, length);
    szCmd[length] = 0x00;

    if (strcmp(szCmd, "auto") == 0)
      SwitchControlMode(eAutomatic);
    else if (strcmp(szCmd, "on") == 0)
      SwitchControlMode(eOn);
    else if (strcmp(szCmd, "off") == 0)
      SwitchControlMode(eOff);
    else
      _log(E, "Unknown CmdSwitch value %s", szCmd);
  }
}

void CFanControl::SetOutput(bool bOn) {
  digitalWrite(m_nPin, bOn);
  m_bIsOn = bOn;
  m_pMqtt_FanState->setValue(bOn ? "on" : "off");

  if (m_pDisplayLine != NULL) {
    char szTmp[64] = "";
    snprintf(szTmp, sizeof(szTmp), "mode:%s state:%s",
             m_pMqtt_ControlMode->getValue().c_str(), bOn ? "on" : "off");
    m_pDisplayLine->Line(szTmp);
  }
}
