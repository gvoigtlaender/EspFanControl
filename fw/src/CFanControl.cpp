#include <CFanControl.h>
#include <CLed.h>
#include <CSensor.h>
#include <CWifi.h>
#include <config.h>
#include <ctime>
#include <CMqtt.h>
#include <CConfigValue.h>
#include <CXbm.h>


CFanControl::CFanControl(int nPin)
    : CControl("CFanCtrl"), m_eControlState(eInit),
      m_eFanControlMode(eAutomatic), m_nPin(nPin) {

  pinMode(nPin, OUTPUT);
  digitalWrite(nPin, LOW);

  m_pTempFanOn = CreateConfigKey<double>("FanControl", "TempFanOn", 25);

  m_pTempFanOff = CreateConfigKey<double>("FanControl", "TempFanOff", 23);
}

bool CFanControl::setup() {

  m_pMqtt_ControlMode = CreateMqttValue("ControlMode", "Automatic");
  m_pMqtt_FanState = CreateMqttValue("FanState", "Unknown");
  m_pMqtt_FanStateB = CreateMqttValue("FanStateB", "0");
  m_pMqtt_Temperature = CreateMqttValue("Temperature", "");

  m_pMqtt_CmdSwitch = CreateMqttCmd("CmdSwitch");

  SwitchControlMode(eAutomatic);

  if (m_pXBM_Temp) {
    m_pXBM_Temp->m_uiUpdateIntervalS = 40;
    m_Values[E_VALUES::e5m].m_sName = "5m";
    m_Values[E_VALUES::e5m].m_Cycle = 5 * 60 / m_pXBM_Temp->m_uiW;
    m_Values[E_VALUES::e30m].m_sName = "30m";
    m_Values[E_VALUES::e30m].m_Cycle = 30 * 60 / m_pXBM_Temp->m_uiW;
    m_Values[E_VALUES::e1h].m_sName = "1h";
    m_Values[E_VALUES::e1h].m_Cycle = 1 * 60 * 60 / m_pXBM_Temp->m_uiW;
    m_Values[E_VALUES::e6h].m_sName = "6h";
    m_Values[E_VALUES::e6h].m_Cycle = 6 * 60 * 60 / m_pXBM_Temp->m_uiW;
    m_Values[E_VALUES::e12h].m_sName = "12h";
    m_Values[E_VALUES::e12h].m_Cycle = 12 * 60 * 60 / m_pXBM_Temp->m_uiW;
    m_Values[E_VALUES::e24h].m_sName = "24h";
    m_Values[E_VALUES::e24h].m_Cycle = 24 * 60 * 60 / m_pXBM_Temp->m_uiW;

    for (uint8_t k = 0; k < E_VALUES::eMax; k++) {
      m_Values[k].m_ValuesT.reserve(m_pXBM_Temp->m_uiW);
      m_Values[k].m_ValuesF.reserve(m_pXBM_Temp->m_uiW);
    }
  }
  return true;
}

//! task control
void CFanControl::control(bool bForce /*= false*/) {
  if (m_uiTime > millis() || m_pDS18B20 == nullptr)
    return;

  switch (m_eControlState) {
  case eInit:
    m_uiTime = millis();
    _log2(I, "eInit");

    if (m_pXBM_Temp) {
      m_pXBM_Temp->m_uiTimer = millis();
    }
    updateStateString();
    m_eControlState = eControl;
    break;

  case eControl:

    double dTemp = m_pDS18B20->GetTemperature(0);
    if (dTemp < 10.0) {
      _log(W, "skip for temp < 10C");
      break;
    }
    if (m_eFanControlMode == eAutomatic) {

      std::string sTmp(FormatString<16>("%.1f", dTemp));
      m_pMqtt_Temperature->setValue(sTmp);
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
    if (m_pXBM_Temp && millis() > m_pXBM_Temp->m_uiTimer) {

      uint8_t y = temp2Y(dTemp);

      for (uint8_t k = 0; k < E_VALUES::eMax; k++) {
        if (m_Values[k].m_millis < millis()) {
          m_Values[k].push_back(y, m_bIsOn, m_pXBM_Temp->m_uiW);
          if (m_ValuesAct == &m_Values[k]) {
            UpdateXBM(false);
          }
          m_Values[k].m_millis += m_Values[k].m_Cycle * 1000;
        }
      }
      m_pXBM_Temp->m_uiTimer += 100;
      // _log(I, "update display in %.1fs", dSecs);
    }
    break;
  }
  m_uiTime += 100;

  static uint8 uiModulo = 0;
  if (++uiModulo % 100 == 0)
    CLed::AddBlinkTask(CLed::BLINK_1);
}

uint8_t CFanControl::temp2Y(double dTemp) {
  const double dDiff = m_pTempFanOn->GetValue() - m_pTempFanOff->GetValue();
  const double dMax = m_pTempFanOn->GetValue() + 2.0 * dDiff;
  const double dOffs = m_pTempFanOff->GetValue() - 2.0 * dDiff;
  return m_pXBM_Temp->m_uiH * (1.0 - (dTemp - dOffs) / (dMax - dOffs));
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
void CFanControl::OnButtonLongClick() {
  m_eValue = (E_VALUES)((m_eValue + 1) % E_VALUES::eMax);
  m_ValuesAct = &m_Values[m_eValue];
  updateStateString();
  UpdateXBM(true);
}

void CFanControl::SwitchControlMode(E_FANCONTROLMODE eMode) {
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
  updateStateString();
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
  m_pMqtt_FanStateB->setValue(bOn ? "1" : "0");
  updateStateString();
}

void CFanControl::updateStateString() {
  if (m_pDisplayLine != NULL) {
    string state = m_pMqtt_ControlMode->getValue() + " " + m_ValuesAct->m_sName;
    m_pDisplayLine->Line(state);
  }
}

void CFanControl::UpdateXBM(bool bForce) {
  if (!m_pXBM_Temp)
    return;

  // _log(D, "UpdateXBM %s", m_ValuesAct->m_sName.c_str());

  m_pXBM_Temp->Clear();
  m_pXBM_Temp->FromVectorI(m_ValuesAct->m_ValuesT);
  for (uint8_t n = 0; n < m_ValuesAct->m_ValuesF.size(); n++) {
    if (m_ValuesAct->m_ValuesF[n]) {
      m_pXBM_Temp->SetPixel(n, 0);
    } else {
      m_pXBM_Temp->SetPixel(n, m_pXBM_Temp->m_uiH - 1);
    }
    if (n > 0 && m_ValuesAct->m_ValuesF[n] != m_ValuesAct->m_ValuesF[n - 1]) {
      for (uint8_t y = 0; y < m_pXBM_Temp->m_uiH; y++)
        m_pXBM_Temp->SetPixel(n, y);
    }
  }

  m_pXBM_Temp->SetPixel(0, temp2Y(m_pTempFanOn->GetValue()));
  m_pXBM_Temp->SetPixel(0, temp2Y(m_pTempFanOff->GetValue()));
  m_pXBM_Temp->SetPixel(m_pXBM_Temp->m_uiW - 1,
                        temp2Y(m_pTempFanOn->GetValue()));
  m_pXBM_Temp->SetPixel(m_pXBM_Temp->m_uiW - 1,
                        temp2Y(m_pTempFanOff->GetValue()));
}