#include "BLEManager.h"

void BLEManager::begin(const char* name){
    if(active) return;
    LOG("[BLE] Baslatiliyor");
    BTstack.setup(name);
    updateAdv();
    active = true;
    LOG("[BLE] Aktif");
}

void BLEManager::updateCompany(uint16_t companyId){
    advData[5] = companyId & 0xFF;
    advData[6] = (companyId >> 8) & 0xFF;
    updateAdv();
    LOGF("[BLE] CompanyID guncellendi %04X\n", companyId);
}

void BLEManager::updateName(const char* name){
    // Keeping name static sized; if name length differs, a more dynamic builder would be needed.
    // For simplicity (karmaşıklık artırmadan) atlanıyor.
    (void) name;
}

void BLEManager::stop(){ if(!active) return; LOG("[BLE] Durduruluyor"); BTstack.stopAdvertising(); active=false; }

void BLEManager::loop(){ if(active) BTstack.loop(); }

bool BLEManager::isActive() const { return active; }

void BLEManager::updateAdv(){
    BTstack.setAdvData(sizeof(advData), advData);
    BTstack.stopAdvertising();
    delay(50);
    BTstack.startAdvertising();
}
