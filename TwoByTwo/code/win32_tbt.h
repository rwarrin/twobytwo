#ifndef WIN32_TBT_H

#define UI_BUTTON_CANCEL     (WM_USER + 200)
#define UI_BUTTON_DELETE     (WM_USER + 201)
#define UI_BUTTON_CREATE     (WM_USER + 202)
#define UI_BUTTON_SAVEAS     (WM_USER + 203)
#define UI_BUTTON_MOVEUP     (WM_USER + 204)
#define UI_BUTTON_MOVEDOWN   (WM_USER + 205)
#define UI_BUTTON_MOVETOP    (WM_USER + 206)
#define UI_BUTTON_MOVEBOTTOM (WM_USER + 207)
#define UI_BUTTON_URGENT     (WM_USER + 210)
#define UI_BUTTON_IMPORTANT  (WM_USER + 211)

#define APP_MENU_FILE_SAVE (WM_USER + 1000)
#define APP_MENU_FILE_LOAD (WM_USER + 1010)
#define APP_MENU_FILE_EXIT (WM_USER + 1020)
#define APP_UI_COMBOBOX_STATUS (WM_USER + 2000)


#define TIMER_AUTOSAVE (WM_USER + 2000)

enum list_view_column
{
    ListViewColumn_Title,
    ListViewColumn_Description,
    ListViewColumn_DueDate,
    ListViewColumn_Status,

    ListViewColumn_Count,
};

enum task_list_view
{
    TaskWindow_UrgentImportant,
    TaskWindow_UrgentNotImportant,
    TaskWindow_NotUrgentImportant,
    TaskWindow_NotUrgentNotImportant,

    TaskWindow_Count,

    TaskWindow_Null = -1,
};

struct task_view
{
    u32 ListMaxCount;
    u32 ListCount;
    task **List;
};

struct edit_task
{
    b32 IsValid;
    u32 ListID;
    u32 ListIndex;
    task *Task;
};


#define WIN32_TBT_H
#endif
