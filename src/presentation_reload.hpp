#pragma once
#include "common.hpp"

// Ask the host to run its own Lost Device cleanup; never Reset behind its back.
class PresentationReload {
    D3DMULTISAMPLE_TYPE native_ = D3DMULTISAMPLE_NONE;
    DWORD nativeQuality_ = 0;
    int observed_ = 0;
    bool pending_ = false;

  public:
    void initialize(const D3DPRESENT_PARAMETERS &p, int choice) {
        native_ = p.MultiSampleType;
        nativeQuality_ = p.MultiSampleQuality;
        observed_ = choice;
    }
    bool changed(int choice) const {
        return choice != observed_;
    }
    void observe(int choice) {
        observed_ = choice;
    }
    void request(int choice) {
        observed_ = choice;
        pending_ = true;
    }
    bool pending() const {
        return pending_;
    }
    D3DMULTISAMPLE_TYPE expected(int choice) const {
        return choice == 0 ? native_ : choice < 0 ? D3DMULTISAMPLE_NONE : D3DMULTISAMPLE_TYPE(choice);
    }
    HRESULT cooperative(HRESULT actual) const {
        return pending_ && actual == D3D_OK ? D3DERR_DEVICENOTRESET : actual;
    }
    void parameters(D3DPRESENT_PARAMETERS &p, int choice) const {
        if (choice == 0) {
            p.MultiSampleType = native_;
            p.MultiSampleQuality = nativeQuality_;
        }
    }
    void complete(HRESULT result) {
        if (SUCCEEDED(result))
            pending_ = false;
    }
};
