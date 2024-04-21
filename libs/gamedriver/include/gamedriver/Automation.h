#ifndef __AUTOMATION_H__
#define __AUTOMATION_H__ 1

/*!
 * \file Automation.h
 * \date 2023.09.07
 *
 * \author Helohuka
 * 
 * Contact: helohuka@outlook.com
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note
*/

#include "gamedriver/BaseLibs.h"


class Automation 
{
public:
    Automation();
    ~Automation();
public:
    void                                setup();
    void                                tick(filament::View* view, filament::gltfio::FilamentInstance* instance, filament::Renderer* renderer, long long dt);
    filament::viewer::AutomationEngine& getAutomationEngine() { return *mAutomationEngine; }

private:
    std::string                         mBatchFile;
    filament::viewer::AutomationSpec*   mAutomationSpec   = nullptr;
    filament::viewer::AutomationEngine* mAutomationEngine = nullptr;
};


#endif // __AUTOMATION_H__
