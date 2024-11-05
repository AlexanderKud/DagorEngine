// Copyright (C) Gaijin Games KFT.  All rights reserved.
#pragma once

#include <EditorCore/ec_interface.h>
#include <EditorCore/ec_interface_ex.h>
#include <math/dag_Point3.h>
#include <math/dag_bounds3.h>
#include <winGuiWrapper/wgw_dialogs.h>
#include "av_script_panel.h"

namespace PropPanel
{
class ContainerPropertyControl;
}

class DagorAsset;
class CoolConsole;
class IWndManager;
class AvTree;


class IGenEditorPlugin : public IGenEditorPluginBase, public IGenEventHandler, public IRenderingService
{
public:
  IGenEditorPlugin() : spEditor(NULL) {}
  virtual ~IGenEditorPlugin() { del_it(spEditor); }

  virtual const char *getInternalName() const = 0;

  virtual void registered() = 0;
  virtual void unregistered() = 0;

  // Plugins can save and load their settings with these functions. Each plugin has its own block within the main
  // application settings, so any name can be used freely in the provided DataBlock.
  virtual void loadSettings(const DataBlock &settings) {}
  virtual void saveSettings(DataBlock &settings) const {}

  virtual bool begin(DagorAsset *asset) = 0;
  virtual bool end() = 0;
  virtual IGenEventHandler *getEventHandler() { return this; }

  // called when user requests switch to this plugin
  virtual void registerMenuAccelerators() {}

  virtual void handleViewportAcceleratorCommand(IGenViewportWnd &wnd, unsigned id) {}

  virtual bool reloadOnAssetChanged(const DagorAsset *changed_asset) { return false; }
  virtual bool reloadAsset(DagorAsset *asset) { return false; }

  virtual bool havePropPanel() { return false; }
  virtual bool haveToolPanel() { return false; }

  virtual void clearObjects() = 0;
  virtual void onSaveLibrary() = 0;
  virtual void onLoadLibrary() = 0;

  virtual bool getSelectionBox(BBox3 &box) const = 0;
  virtual void actObjects(real dt) = 0;
  virtual void beforeRenderObjects() = 0;
  virtual void renderObjects() = 0;
  virtual void renderTransObjects() = 0;
  virtual void renderUI() {}
  virtual void updateImgui() {}

  virtual bool supportAssetType(const DagorAsset &asset) const { return false; }
  virtual bool supportEditing() const { return true; }

  virtual void fillPropPanel(PropPanel::ContainerPropertyControl &propPanel) = 0;
  virtual void postFillPropPanel() = 0;
  virtual void onPropPanelClear(PropPanel::ContainerPropertyControl &propPanel);

  virtual bool hasScriptPanel();
  virtual void fillScriptPanel(PropPanel::ContainerPropertyControl &propPanel);
  void initScriptPanelEditor(const char *scheme, const char *panel_caption = NULL);

  // IGenEventHandler
  //===========================================================================
  virtual void handleKeyPress(IGenViewportWnd *wnd, int vk, int modif) {}
  virtual void handleKeyRelease(IGenViewportWnd *wnd, int vk, int modif) {}

  virtual bool handleMouseMove(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseLBPress(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseLBRelease(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseRBPress(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseRBRelease(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseCBPress(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseCBRelease(IGenViewportWnd *wnd, int x, int y, bool inside, int buttons, int key_modif) { return false; }
  virtual bool handleMouseWheel(IGenViewportWnd *wnd, int wheel_d, int x, int y, int key_modif) { return false; }
  virtual bool handleMouseDoubleClick(IGenViewportWnd *wnd, int x, int y, int key_modif) { return false; }

  virtual void handleViewportPaint(IGenViewportWnd *wnd) { drawInfo(wnd); }
  virtual void handleViewChange(IGenViewportWnd *wnd) {}
  //===========================================================================

  // IGenEditorPluginBase
  virtual bool getVisible() const;
  virtual void *queryInterfacePtr(unsigned huid)
  {
    return huid == IRenderingService::HUID ? static_cast<IRenderingService *>(this) : NULL;
  }

  // IRenderingService
  virtual void renderGeometry(Stage stage) {}

  static void drawInfo(IGenViewportWnd *wnd);
  static void repaintView();
  static PropPanel::ContainerPropertyControl *getPropPanel();
  static PropPanel::ContainerPropertyControl *getPluginPanel();
  static void fillPluginPanel();
  static CoolConsole &getMainConsole();
  static IWndManager &getWndManager();

protected:
  AVScriptPanelEditor *spEditor;
};
