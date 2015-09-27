//-----------------------------------------------------------------------------
// Copyright (c) 2015 Andrew Mac
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

// For compilers that don't support precompilation, include "wx/wx.h"
#include "wx/wxprec.h"
 
#ifndef WX_PRECOMP
#	include "wx/wx.h"
#endif

#include <wx/propgrid/propgrid.h>
#include <wx/dir.h>
#include <wx/treectrl.h>

// UI generated from wxFormBuilder
#include "../Torque6EditorUI.h"

#include "projectTool.h"
#include "3d/scene/camera.h"
#include "module/moduleManager.h"
#include <bx/bx.h>
#include <bx/fpumath.h>

ProjectTool::ProjectTool()
   : mProjectPanel(NULL),
     mSelectedModule(NULL)
{
   mAssetIconList = new wxImageList(16, 16);
}

ProjectTool::~ProjectTool()
{

}

void ProjectTool::initTool()
{
   mProjectPanel = new ProjectPanel(mFrame, wxID_ANY);

   // Entity Icons
   mAssetIconList->Add(wxBitmap("images/moduleIcon.png", wxBITMAP_TYPE_PNG));
   mAssetIconList->Add(wxBitmap("images/iconFolderGrey.png", wxBITMAP_TYPE_PNG));
   mAssetIconList->Add(wxBitmap("images/assetIcon.png", wxBITMAP_TYPE_PNG));
   mProjectPanel->assetList->AssignImageList(mAssetIconList);

   // Entity Events
   mProjectPanel->assetList->Connect(wxID_ANY, wxEVT_TREE_BEGIN_DRAG, wxTreeEventHandler(ProjectTool::OnTreeDrag), NULL, this);
   mProjectPanel->assetList->Connect(wxID_ANY, wxEVT_TREE_ITEM_ACTIVATED, wxTreeEventHandler(ProjectTool::OnTreeEvent), NULL, this);
   mProjectPanel->assetList->Connect(wxID_ANY, wxEVT_TREE_ITEM_MENU, wxTreeEventHandler(ProjectTool::OnTreeMenu), NULL, this);
   mProjectPanel->assetPropGrid->Connect(wxID_ANY, wxEVT_PG_CHANGED, wxPropertyGridEventHandler(ProjectTool::OnPropertyChanged), NULL, this);
   
   // Entity Menu Events
   mProjectPanel->moduleMenu->Connect(wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ProjectTool::OnMenuEvent), NULL, this);

   // Entity List
   mAssetListRoot = mProjectPanel->assetList->AddRoot("ROOT");

   mManager->AddPane(mProjectPanel, wxAuiPaneInfo().Caption("Project")
                                                  .CaptionVisible( true )
                                                  .CloseButton( true )
                                                  .PinButton( true )
                                                  .MaximizeButton(true)
                                                  .Dock()
                                                  .Resizable()
                                                  .FloatingSize( wxDefaultSize )
                                                  .Left()
                                                  .Hide());
   mManager->Update();
}

void ProjectTool::openTool()
{
   wxAuiPaneInfo& paneInfo = mManager->GetPane(mProjectPanel);
   paneInfo.Show();
   mManager->Update();

   if (mProjectManager->mProjectLoaded)
   {
      refreshAssetList();
   }
}

void ProjectTool::closeTool()
{
   wxAuiPaneInfo& paneInfo = mManager->GetPane(mProjectPanel);
   paneInfo.Hide();
   mManager->Update();
}

void ProjectTool::onProjectLoaded(wxString projectName, wxString projectPath)
{
   refreshAssetList();
}

void ProjectTool::onProjectClosed()
{
   //
}

void ProjectTool::OnTreeDrag(wxTreeEvent& evt)
{
   if (evt.GetId() == ASSET_LIST)
   {
      AssetTreeItemData* data = dynamic_cast<AssetTreeItemData*>(mProjectPanel->assetList->GetItemData(evt.GetItem()));
      if (data)
      {
         const AssetDefinition* asset = data->objPtr;

         wxString command("Asset->");
         command.Append(asset->mAssetType);
         command.Append("->");
         command.Append(asset->mAssetId);

         wxTextDataObject dragData(command);
         wxDropSource dragSource(mProjectPanel);
         dragSource.SetData(dragData);
         wxDragResult result = dragSource.DoDragDrop(TRUE);
         return;
      }
   }
}

void ProjectTool::OnTreeEvent( wxTreeEvent& evt )
{
   if (evt.GetId() == ASSET_LIST)
   {
      AssetTreeItemData* data = dynamic_cast<AssetTreeItemData*>(mProjectPanel->assetList->GetItemData(evt.GetItem()));
      if (data)
      {
         loadAssetDefinitionProperties(mProjectPanel->assetPropGrid, data->objPtr);
         return;
      }
   }
}

void ProjectTool::OnTreeMenu( wxTreeEvent& evt )
{ 
   mSelectedModule = NULL;

   if (evt.GetId() == ASSET_LIST)
   {
      ModuleTreeItemData* module_data = dynamic_cast<ModuleTreeItemData*>(mProjectPanel->assetList->GetItemData(evt.GetItem()));
      if (module_data)
      {
         Module mod = module_data->obj;
         mSelectedModule = Plugins::Link.ModuleDatabaseLink->findLoadedModule(mod.moduleID);
         if (mSelectedModule != NULL)
            mFrame->PopupMenu(mProjectPanel->moduleMenu, wxDefaultPosition);
         return;
      }
   }
} 

void ProjectTool::OnMenuEvent(wxCommandEvent& evt)
{
   if (evt.GetId() == MENU_IMPORT_MESH)
   {
      ImportMeshWizard* wizard = new ImportMeshWizard(mFrame);

      // Set initial import path, the user can change it.
      wxString importPath = mSelectedModule->getModulePath();
      importPath.Append("/meshes");
      wizard->importPath->SetPath(importPath);

      if (wizard->RunWizard(wizard->m_pages[0]))
      {
         wxString assetID     = wizard->assetID->GetValue();
         wxString meshPath    = wizard->meshFilePath->GetFileName().GetFullPath();
         wxString meshFile    = wizard->meshFilePath->GetFileName().GetFullName();
         wxString importPath  = wizard->importPath->GetPath();

         // Copy file (optional)
         if (wizard->copyMeshCheck->GetValue())
         {
            wxString moduleMeshPath(importPath);
            moduleMeshPath.Append("/");
            moduleMeshPath.Append(meshFile);

            Plugins::Link.Platform.createPath(moduleMeshPath.c_str());
            Plugins::Link.Platform.pathCopy(meshPath.c_str(), moduleMeshPath.c_str(), false);
            meshPath = moduleMeshPath;
         }

         // Make path relative to module directory.
         char buf[1024];
         const char* fullPath = Plugins::Link.Platform.makeFullPathName(meshPath.c_str(), buf, sizeof(buf), NULL);
         StringTableEntry relativePath = Plugins::Link.Platform.makeRelativePathName(fullPath, importPath);

         // Create full import path.
         importPath.Append("/");
         importPath.Append(assetID);
         importPath.Append(".asset.taml");

         // Create asset definition.
         Plugins::Link.Scene.createMeshAsset(assetID.c_str(), relativePath, importPath.c_str());
         Plugins::Link.AssetDatabaseLink.addDeclaredAsset(mSelectedModule, importPath.c_str());
         refreshAssetList();
      }

      wizard->Destroy();
   }
   if (evt.GetId() == MENU_IMPORT_TEXTURE)
   {
      ImportTextureWizard* wizard = new ImportTextureWizard(mFrame);
      wizard->RunWizard(wizard->m_pages[0]);
      wizard->Destroy();
   }
}

void ProjectTool::OnPropertyChanged( wxPropertyGridEvent& evt )
{ 
   wxString name = evt.GetPropertyName();
   wxVariant val = evt.GetPropertyValue();
   wxString strVal = val.GetString();
}

const char* ProjectTool::getAssetCategoryName(const char* _name)
{
   if (dStrcmp(_name, "EntityTemplateAsset") == 0)
      return "Object Templates";

   if (dStrcmp(_name, "MaterialAsset") == 0)
      return "Materials";

   if (dStrcmp(_name, "MeshAsset") == 0)
      return "Meshes";

   if (dStrcmp(_name, "ImageAsset") == 0)
      return "Images";

   if (dStrcmp(_name, "ShaderAsset") == 0)
      return "Shaders";

   return _name;
}

void ProjectTool::refreshAssetList()
{
   // Clear list.
   mProjectPanel->assetList->DeleteAllItems();
   mAssetListRoot = mProjectPanel->assetList->AddRoot("ROOT");

   Vector<const AssetDefinition*> assetDefinitions = Plugins::Link.AssetDatabaseLink.getDeclaredAssets();
   Vector<Module> modules;

   // Iterate sorted asset definitions.
   for (Vector<const AssetDefinition*>::iterator assetItr = assetDefinitions.begin(); assetItr != assetDefinitions.end(); ++assetItr)
   {
      // Fetch asset definition.
      const AssetDefinition* pAssetDefinition = *assetItr;

      char buf[256];
      dStrcpy(buf, pAssetDefinition->mAssetId);
      const char* moduleName = dStrtok(buf, ":");
      const char* assetName = dStrtok(NULL, ":");

      // Try to find module
      bool foundModule = false;
      for (Vector<Module>::iterator modulesItr = modules.begin(); modulesItr != modules.end(); ++modulesItr)
      {
         const char* moduleID = pAssetDefinition->mpModuleDefinition->getModuleId();
         if (dStrcmp(modulesItr->moduleID, moduleID) == 0)
         {
            // Try to find category
            bool foundCategory = false;
            for (Vector<AssetCategory>::iterator categoriesItr = modulesItr->assets.begin(); categoriesItr != modulesItr->assets.end(); ++categoriesItr)
            {
               const char* moduleID = pAssetDefinition->mpModuleDefinition->getModuleId();
               if (dStrcmp(categoriesItr->categoryName, pAssetDefinition->mAssetType) == 0)
               {
                  categoriesItr->assets.push_back(pAssetDefinition);
                  mProjectPanel->assetList->AppendItem(categoriesItr->treeItemID, assetName, 2, -1, new AssetTreeItemData(pAssetDefinition));
                  foundCategory = true;
                  break;
               }
            }

            // Can't find module? Create one.
            if (!foundCategory)
            {
               AssetCategory newCategory;
               newCategory.categoryName = pAssetDefinition->mAssetType;
               newCategory.treeItemID = mProjectPanel->assetList->AppendItem(modulesItr->treeItemID, getAssetCategoryName(pAssetDefinition->mAssetType), 1, -1, new AssetCategoryTreeItemData(newCategory));

               mProjectPanel->assetList->AppendItem(newCategory.treeItemID, assetName, 2, -1, new AssetTreeItemData(pAssetDefinition));

               newCategory.assets.push_back(pAssetDefinition);
               modulesItr->assets.push_back(newCategory);
            }

            foundModule = true;
            break;
         }
      }

      // Can't find module? Create one.
      if (!foundModule)
      {
         Module newModule;
         newModule.moduleID = pAssetDefinition->mpModuleDefinition->getModuleId();
         newModule.moduleVersion = pAssetDefinition->mpModuleDefinition->getVersionId();

         AssetCategory newCategory;
         newCategory.categoryName = pAssetDefinition->mAssetType;
         
         newModule.treeItemID = mProjectPanel->assetList->AppendItem(mAssetListRoot, newModule.moduleID, 0, -1, new ModuleTreeItemData(newModule));
         newCategory.treeItemID = mProjectPanel->assetList->AppendItem(newModule.treeItemID, getAssetCategoryName(pAssetDefinition->mAssetType), 1, -1, new AssetCategoryTreeItemData(newCategory));
         mProjectPanel->assetList->AppendItem(newCategory.treeItemID, assetName, 2, -1, new AssetTreeItemData(pAssetDefinition));

         newCategory.assets.push_back(pAssetDefinition);
         newModule.assets.push_back(newCategory);
         modules.push_back(newModule);
      }
   }
}

void ProjectTool::loadAssetDefinitionProperties(wxPropertyGrid* propertyGrid, const AssetDefinition* assetDef)
{
   propertyGrid->Clear();

   AssetBase* asset = Plugins::Link.AssetDatabaseLink.getAssetBase(assetDef->mAssetId);

   wxString fieldGroup("");
   bool addFieldGroup = false;

   AbstractClassRep::FieldList fieldList = asset->getFieldList();
   for (Vector<AbstractClassRep::Field>::iterator itr = fieldList.begin(); itr != fieldList.end(); itr++)
   {
      const AbstractClassRep::Field* f = itr;
      if (f->type == AbstractClassRep::DepricatedFieldType ||
         f->type == AbstractClassRep::EndGroupFieldType)
         continue;

      if (f->type == AbstractClassRep::StartGroupFieldType)
      {
         addFieldGroup = true;
         fieldGroup = f->pGroupname;
         continue;
      }

      for (U32 j = 0; S32(j) < f->elementCount; j++)
      {
         const char *val = (*f->getDataFn)(asset, Plugins::Link.Con.getData(f->type, (void *)(((const char *)asset) + f->offset), j, f->table, f->flag));

         if (!val)
            continue;

         if (addFieldGroup)
         {
            propertyGrid->Append(new wxPropertyCategory(fieldGroup));
            addFieldGroup = false;
         }

         if (f->type == Plugins::Link.Con.TypeBool)
            propertyGrid->Append(new wxBoolProperty(f->pFieldname, f->pFieldname, val));
         else
            propertyGrid->Append(new wxStringProperty(f->pFieldname, f->pFieldname, val));
      }
   }



   StringTableEntry texture0 = Plugins::Link.StringTableLink->insert("Texture0");
   propertyGrid->Append(new wxStringProperty("Texture0", "Texture0", asset->getDataField(texture0, NULL)));
   //propertyGrid->Append(new wxStringProperty("Name", "AssetName", assetDef->mAssetName));
   //propertyGrid->Append(new wxStringProperty("Description", "AssetDescription", assetDef->mAssetDescription));
   //propertyGrid->Append(new wxBoolProperty("Auto Unload", "AutoUnload", assetDef->mAssetAutoUnload));
   //propertyGrid->Append(new wxStringProperty("Base File Path", "BaseFilePath", assetDef->mAssetBaseFilePath));
}