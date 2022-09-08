#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <richedit.h>
#include <commctrl.h>
#include <shellapi.h>
#include <malloc.h>

#include "tbt_common.h"
#include "tbt.h"
#include "win32_tbt.h"

#include "tbt.cpp"

static task_list *GlobalTaskList = 0;
static memory_arena *Arena = 0;

static HWND TaskWindows[TaskWindow_Count] = {0};
static task_view TaskViews[TaskWindow_Count] = {0};
static char GlobalSaveFileName[MAX_PATH] = {0};
static edit_task GlobalEditTask = {0};

static HWND GlobalStatusBarHandle = 0;
static HWND GlobalStaticTitleWindow = 0;
static HWND GlobalEditTitleWindow = 0;
static HWND GlobalStaticDescriptionWindow = 0;
static HWND GlobalEditDescriptionWindow = 0;
static HWND GlobalStaticDueDateWindow = 0;
static HWND GlobalEditDueDateWindow = 0;
static HWND GlobalStaticStatusWindow = 0;
static HWND GlobalComboBoxStatusWindow = 0;
static HWND GlobalStaticTypeWindow = 0;
static HWND GlobalTypeUrgentWindow = 0;
static HWND GlobalTypeImportantWindow = 0;
static HWND GlobalCancelFormButton = 0;
static HWND GlobalCreateNewFormButton = 0;
static HWND GlobalSaveFormButton = 0;
static HWND GlobalDeleteFormButton = 0;
static HWND GlobalMoveTaskUpButton = 0;
static HWND GlobalMoveTaskDownButton = 0;
static HWND GlobalMoveTaskTopButton = 0;
static HWND GlobalMoveTaskBottomButton = 0;
static HWND GlobalStaticMoveWindow = 0;

static NOTIFYICONDATA GlobalTrayIconHandle = {0};

static char *GlobalStatusStrings[TaskState_Count] = {
    "Not Started",
    "In Progress",
    "Complete",
    "Removed",
};

inline void
BeginEditingTask(edit_task *EditTask, u32 ViewListID, u32 ViewListIndex, task *Task)
{
    EditTask->IsValid = true;
    EditTask->ListID = ViewListID;
    EditTask->ListIndex = ViewListIndex;
    EditTask->Task = Task;
    Button_Enable(GlobalSaveFormButton, true);
    Button_Enable(GlobalDeleteFormButton, true);
    Button_Enable(GlobalMoveTaskUpButton, true);
    Button_Enable(GlobalMoveTaskDownButton, true);
    Button_Enable(GlobalMoveTaskTopButton, true);
    Button_Enable(GlobalMoveTaskBottomButton, true);
}

inline void
EndEditingTask(edit_task *EditTask)
{
    Button_Enable(GlobalSaveFormButton, false);
    Button_Enable(GlobalDeleteFormButton, false);
    Button_Enable(GlobalMoveTaskUpButton, false);
    Button_Enable(GlobalMoveTaskDownButton, false);
    Button_Enable(GlobalMoveTaskTopButton, false);
    Button_Enable(GlobalMoveTaskBottomButton, false);
    *EditTask = {0};
}

static task_view *
GetTaskViewForTask(task *Task)
{
    task_view *View = &TaskViews[TaskWindow_Null];
    if(Task->TaskUrgency == TaskUrgency_Urgent)
    {
        if(Task->TaskImportance == TaskImportance_Important)
        {
            View = &TaskViews[TaskWindow_UrgentImportant];
        }
        else if(Task->TaskImportance == TaskImportance_NotImportant)
        {
            View = &TaskViews[TaskWindow_UrgentNotImportant];
        }
    }
    else if(Task->TaskUrgency == TaskUrgency_NotUrgent)
    {
        if(Task->TaskImportance == TaskImportance_Important)
        {
            View = &TaskViews[TaskWindow_NotUrgentImportant];
        }
        else if(Task->TaskImportance == TaskImportance_NotImportant)
        {
            View = &TaskViews[TaskWindow_NotUrgentNotImportant];
        }
    }

    return(View);
}

static u32
GetViewListIndexForTask(task *Task)
{
    u32 Result = 0;
    if(Task->TaskUrgency == TaskUrgency_Urgent)
    {
        if(Task->TaskImportance == TaskImportance_Important)
        {
            Result = TaskWindow_UrgentImportant;
        }
        else if(Task->TaskImportance == TaskImportance_NotImportant)
        {
            Result = TaskWindow_UrgentNotImportant;
        }
    }
    else if(Task->TaskUrgency == TaskUrgency_NotUrgent)
    {
        if(Task->TaskImportance == TaskImportance_Important)
        {
            Result = TaskWindow_NotUrgentImportant;
        }
        else if(Task->TaskImportance == TaskImportance_NotImportant)
        {
            Result = TaskWindow_NotUrgentNotImportant;
        }
    }

    return(Result);
}

static void
AddTaskToTaskView(task_view *View, task *Task)
{
    if((View->ListCount + 1) >= View->ListMaxCount)
    {
        u32 NewListSize = (View->ListMaxCount*2);
        task **NewList = (task **)malloc(sizeof(*NewList) * NewListSize);
        Assert(NewList);

        Copy(NewList, sizeof(*NewList)*View->ListCount, View->List);

        free(View->List);
        View->List = NewList;
        View->ListMaxCount = NewListSize;
    }

    View->List[View->ListCount++] = Task;
}

static void
RemoveTaskFromTaskView(task_view *TaskView, u32 RemoveAt)
{
    u32 ListMax = TaskView->ListCount - 1;
    for(u32 ListIndex = RemoveAt; ListIndex < ListMax; ++ListIndex)
    {
        TaskView->List[ListIndex] = TaskView->List[ListIndex + 1];
    }
    TaskView->List[--TaskView->ListCount] = 0;
}

static void
Win32CreateMenu(HWND Window)
{
    HMENU MainMenu = CreateMenu();

    HMENU FileMenu = CreateMenu();
    AppendMenuA(FileMenu, MF_STRING, APP_MENU_FILE_SAVE, "&Save");
    AppendMenuA(FileMenu, MF_STRING, APP_MENU_FILE_LOAD, "&Open");
    AppendMenuA(FileMenu, MF_SEPARATOR, 0, 0);
    AppendMenuA(FileMenu, MF_STRING, APP_MENU_FILE_EXIT, "&Exit");

    AppendMenuA(MainMenu, MF_POPUP, (UINT_PTR)FileMenu, "&File");

    SetMenu(Window, MainMenu);
}

static void
ResetTaskInputForm()
{
    Edit_SetText(GlobalEditTitleWindow, "");
    Edit_SetText(GlobalEditDescriptionWindow, "");
    Edit_SetText(GlobalEditDueDateWindow, "");
    Button_SetCheck(GlobalTypeUrgentWindow, false);
    Button_SetCheck(GlobalTypeImportantWindow, false);
    ComboBox_SetCurSel(GlobalComboBoxStatusWindow, 0);
}

inline void
QuickSortTasks(task **List, s32 Left, s32 Right)
{
    if(Left < Right)
    {
        task *PartitionTask = List[Right];
        
        s32 Partition = Left - 1;
        for(s32 Index = Left; Index < Right; ++Index)
        {
            if(List[Index]->TaskID <= PartitionTask->TaskID)
            {
                ++Partition;
                Swap(&List[Partition], &List[Index]);
            }
        }
        Swap(&List[Partition + 1], &List[Right]);

        QuickSortTasks(List, Left, Partition);
        QuickSortTasks(List, Partition + 1, Right);
    }
}

static void
SortTaskViewByID(task_view *TaskView)
{
    if(TaskView->ListCount)
    {
        if(TaskView->ListCount > 50)
        {
            // NOTE(rick): Use a fisher yates shuffle to avoid a degenerate case
            // where we quicksort an already sorted list causing a worse case
            // running time.  Using 50 now for no particular reason maybe there's a
            // better task count where we should shuffle first but this works for
            // now.
            for(u32 Index = TaskView->ListCount - 1; Index >= 1; --Index)
            {
                u32 RandomIndex = rand() % Index;
                Swap(&TaskView->List[Index], &TaskView->List[RandomIndex]);
            }
        }

        QuickSortTasks(TaskView->List, 0, TaskView->ListCount - 1);

        // TODO(rick): Remove this assertion for release
        for(u32 TaskIndex = 0; TaskIndex < (TaskView->ListCount - 1); ++TaskIndex)
        {
            Assert(TaskView->List[TaskIndex]->TaskID <= TaskView->List[TaskIndex + 1]->TaskID);
        }
    }
}

static void
PopulateViewsFromTaskList(task_list *TaskList)
{
    for(u32 ListIndex = 0; ListIndex < TaskWindow_Count; ++ListIndex)
    {
        task_view *TaskView = TaskViews + ListIndex;
        for(u32 TaskIndex = 0; TaskIndex < TaskView->ListCount; ++TaskIndex)
        {
            TaskView->List[TaskIndex] = 0;
        }
        TaskView->ListCount = 0;
    }

    for(task_list *List = TaskList; List; List = List->Next)
    {
        for(u32 ListIndex = 0; ListIndex < List->TaskCount; ++ListIndex)
        {
            task *Task = List->Tasks[ListIndex];
            if(Task->TaskState < TaskState_Removed)
            {
                task_view *View = GetTaskViewForTask(Task);
                Assert(View != &TaskViews[TaskWindow_Null]);

                AddTaskToTaskView(View, Task);
            }
        }

        if(List->Next == TaskList)
        {
            break;
        }
    }

    for(u32 ListIndex = 0; ListIndex < TaskWindow_Count; ++ListIndex)
    {
        task_view *TaskView = TaskViews + ListIndex;

        SortTaskViewByID(TaskView);

        HWND TaskWindow = TaskWindows[ListIndex];
        ListView_SetItemCount(TaskWindow, TaskView->ListCount);
        ListView_RedrawItems(TaskWindow, 0, TaskView->ListCount);
        UpdateWindow(TaskWindow);
    }
}

static void
UpdateTaskFromUIInputs(task *Task)
{
    b32 UrgentState = Button_GetCheck(GlobalTypeUrgentWindow);
    task_urgency TaskUrgency = (UrgentState ? TaskUrgency_Urgent : TaskUrgency_NotUrgent);

    b32 ImportantState = Button_GetCheck(GlobalTypeImportantWindow);
    task_importance TaskImportance = (ImportantState ? TaskImportance_Important : TaskImportance_NotImportant);

    s32 SelectedStatusIndex = ComboBox_GetCurSel(GlobalComboBoxStatusWindow);
    task_state TaskState = (task_state)(TaskState_NotStarted + SelectedStatusIndex);

    char *Title = (char *)_malloca(Task->TitleMaxLength);
    char *Description = (char *)_malloca(Task->DescriptionMaxLength);
    char *DueDate = (char *)_malloca(Task->DueDateMaxLength);
    ZeroSize(Title, Task->TitleMaxLength);
    ZeroSize(Description, Task->DescriptionMaxLength);
    ZeroSize(DueDate, Task->DueDateMaxLength);
    Edit_GetText(GlobalEditTitleWindow, Title, Task->TitleMaxLength);
    Edit_GetText(GlobalEditDescriptionWindow, Description, Task->DescriptionMaxLength);
    Edit_GetText(GlobalEditDueDateWindow, DueDate, Task->DueDateMaxLength);

    UpdateTask(Task, TaskState, TaskUrgency, TaskImportance,
               Title, Description, DueDate);
#if 0
    Task->TitleLength = CopyNullTerminatedString(Task->Title, Title);
    Task->DescriptionLength = CopyNullTerminatedString(Task->Description, Description);
    Task->DueDateLength = CopyNullTerminatedString(Task->DueDate, DueDate);
    Task->TaskState = TaskState;
    Task->TaskUrgency = TaskUrgency;
    Task->TaskImportance = TaskImportance;
#endif
}

static NOTIFYICONDATA
ShowTrayIcon(HWND Window)
{
    HICON Icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));

    NOTIFYICONDATAA NotifyIconData = {0};
    NotifyIconData.cbSize = sizeof(NotifyIconData);
    NotifyIconData.hWnd = Window;
    NotifyIconData.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
    NotifyIconData.uCallbackMessage = UI_TRAY_COMMAND;
    NotifyIconData.hIcon = Icon;
    strncpy(NotifyIconData.szTip, "2x2 - Task Manager", ArrayCount(NotifyIconData.szTip));

    Shell_NotifyIcon(NIM_ADD, &NotifyIconData);

    return(NotifyIconData);
}

static void
RemoveTrayIcon(NOTIFYICONDATA *TrayIcon)
{
    Shell_NotifyIcon(NIM_DELETE, TrayIcon);
}

LRESULT
Win32Callback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_DESTROY:
        {
            OutputDebugStringA("WM_DESTROY\n");
            PostQuitMessage(0);
        } break;

        case WM_NOTIFY:
        {
            NMHDR *NotificationMessageHeader = (NMHDR *)LParam;
            HWND ListWindowHandle = NotificationMessageHeader->hwndFrom;
            UINT ListWindowID = NotificationMessageHeader->idFrom;
            UINT NotificationCode = NotificationMessageHeader->code;
            switch(NotificationCode)
            {
                case LVN_GETDISPINFO:
                {
                    NMLVDISPINFO *DisplayInfo = (NMLVDISPINFO *)LParam;
                    LVITEMA *Item = &DisplayInfo->item;

                    Assert((ListWindowID >= 0) && (ListWindowID < TaskWindow_Count));
                    task_view *TaskView = TaskViews + ListWindowID;

                    s32 ListOffset = Item->iItem;
                    Assert((ListOffset >= 0) && (ListOffset < TaskView->ListCount));
                    task *Task = TaskView->List[ListOffset];

                    char Text[256] = {0};
                    snprintf(Text, ArrayCount(Text), "Got LVN_GETDISPINFO, hwnd: %zd, id: %d, row: %d, iSubItem: %d\n", (size_t)ListWindowHandle, ListWindowID, ListOffset, Item->iSubItem);
                    OutputDebugStringA(Text);

                    switch(Item->iSubItem)
                    {
                        case ListViewColumn_Title:
                        {
                            if(Item->mask & LVIF_TEXT)
                            {
                                Item->pszText = Task->Title;
                            }
                        } break;

                        case ListViewColumn_Description:
                        {
                            if(Item->mask & LVIF_TEXT)
                            {
                                Item->pszText = Task->Description;
                            }
                        } break;

                        case ListViewColumn_DueDate:
                        {
                            if(Item->mask & LVIF_TEXT)
                            {
                                Item->pszText = Task->DueDate;
                            }
                        } break;

                        case ListViewColumn_Status:
                        {
                            if(Item->mask & LVIF_TEXT)
                            {
                                switch(Task->TaskState)
                                {
                                    case TaskState_NotStarted:
                                    case TaskState_InProgress:
                                    case TaskState_Complete:
                                    {
                                        Item->pszText = GlobalStatusStrings[Task->TaskState];
                                    } break;

                                    InvalidDefaultCase;
                                }
                            }
                        } break;

                        InvalidDefaultCase;
                    }
                } break;

                case NM_DBLCLK:
                {
                    NMITEMACTIVATE *ItemActivate = (NMITEMACTIVATE *)LParam;
                    NMHDR Header = ItemActivate->hdr;

                    u32 ViewListID = Header.idFrom;
                    s32 ListIndex = ItemActivate->iItem;
                    if(ListIndex >= 0)
                    {
                        char Text[256] = {0};
                        snprintf(Text, ArrayCount(Text), "NM_DBLCLK: id:%d, index:%d\n", ViewListID, ListIndex);
                        OutputDebugStringA(Text);

                        task_view *TaskView = TaskViews + ViewListID;
                        task *Task = *(TaskView->List + ListIndex);

                        Edit_SetText(GlobalEditTitleWindow, Task->Title);
                        Edit_SetText(GlobalEditDescriptionWindow, Task->Description);
                        Edit_SetText(GlobalEditDueDateWindow, Task->DueDate);
                        Button_SetCheck(GlobalTypeUrgentWindow, (Task->TaskUrgency == TaskUrgency_Urgent));
                        Button_SetCheck(GlobalTypeImportantWindow, (Task->TaskImportance == TaskImportance_Important));
                        ComboBox_SetCurSel(GlobalComboBoxStatusWindow, Task->TaskState);

                        BeginEditingTask(&GlobalEditTask, ViewListID, ListIndex, Task);
                    }
                } break;
            }
        } break;

        case WM_COMMAND:
        {
            s32 NotificationCode = (s32)HIWORD(WParam);
            s32 CommandIdentifier = (s32)LOWORD(WParam);
            switch(CommandIdentifier)
            {
                case UI_BUTTON_CANCEL:
                {
                    EndEditingTask(&GlobalEditTask);
                    ResetTaskInputForm();
                    OutputDebugStringA("UI_BUTTON_CANCEL\n");
                } break;

                case UI_BUTTON_DELETE:
                {
                    if(GlobalEditTask.IsValid)
                    {
                        task_view *TaskView = TaskViews + GlobalEditTask.ListID;
                        RemoveTaskFromTaskView(TaskView, GlobalEditTask.ListIndex);

                        task *TaskToDelete = GlobalEditTask.Task;
                        ClearAndRemoveTaskFromTaskList(GlobalTaskList, TaskToDelete);

                        HWND TaskWindow = TaskWindows[GlobalEditTask.ListID];
                        ListView_SetItemCount(TaskWindow, TaskView->ListCount);
                        ListView_RedrawItems(TaskWindow, 0, TaskView->ListCount);
                        UpdateWindow(TaskWindow);

                        EndEditingTask(&GlobalEditTask);
                        ResetTaskInputForm();
                    }

                    OutputDebugStringA("UI_BUTTON_DELETE\n");
                } break;

                case UI_BUTTON_CREATE:
                {
                    s32 HasTitle = Edit_GetTextLength(GlobalEditTitleWindow);
                    if(HasTitle)
                    {
                        task *Task = MakeEmptyTask(Arena);
                        if(Task)
                        {
                            TaskListInsert(GlobalTaskList, Task);

                            UpdateTaskFromUIInputs(Task);

                            u32 ViewListIndex = GetViewListIndexForTask(Task);
                            task_view *TaskView = TaskViews + ViewListIndex;
                            AddTaskToTaskView(TaskView, Task);

                            HWND TaskWindow = *(TaskWindows + ViewListIndex);
                            ListView_SetItemCount(TaskWindow, TaskView->ListCount);
                            ListView_RedrawItems(TaskWindow, 0, TaskView->ListCount);
                            UpdateWindow(TaskWindow);

                            BeginEditingTask(&GlobalEditTask, ViewListIndex, (TaskView->ListCount - 1), Task);
                        }
                    }

                    OutputDebugStringA("UI_BUTTON_CREATE\n");
                } break;

                case UI_BUTTON_SAVEAS:
                {
                    if(GlobalEditTask.IsValid)
                    {
                        Assert(GlobalEditTask.Task);
                        task *Task = GlobalEditTask.Task;

                        UpdateTaskFromUIInputs(Task);

                        u32 NewTaskListID = GetViewListIndexForTask(Task);
                        task_view *TaskView = (TaskViews + GlobalEditTask.ListID);

                        HWND TaskWindow = *(TaskWindows + GlobalEditTask.ListID);
                        if(NewTaskListID != GlobalEditTask.ListID)
                        {
                            task_view *NewTaskView = (TaskViews + NewTaskListID);
                            HWND NewTaskWindow = *(TaskWindows + NewTaskListID);

                            RemoveTaskFromTaskView(TaskView, GlobalEditTask.ListIndex);

                            AddTaskToTaskView(NewTaskView, Task);

                            SortTaskViewByID(NewTaskView);

                            ListView_SetItemCount(TaskWindow, TaskView->ListCount);
                            ListView_RedrawItems(TaskWindow, 0, TaskView->ListCount);
                            ListView_SetItemCount(NewTaskWindow, NewTaskView->ListCount);
                            ListView_RedrawItems(NewTaskWindow, 0, NewTaskView->ListCount);
                            UpdateWindow(TaskWindow);
                            UpdateWindow(NewTaskWindow);

                            BeginEditingTask(&GlobalEditTask, NewTaskListID, (NewTaskView->ListCount - 1), Task);
                        }
                        else
                        {
                            ListView_RedrawItems(TaskWindow, 0, TaskView->ListCount);
                            UpdateWindow(TaskWindow);
                        }
                    }

                    OutputDebugStringA("UI_BUTTON_SAVEAS\n");
                } break;

                case UI_BUTTON_MOVEUP:
                {
                    if(GlobalEditTask.IsValid)
                    {
                        task_view *TaskView = TaskViews + GlobalEditTask.ListID;
                        HWND TaskWindow = TaskWindows[GlobalEditTask.ListID];

                        if(GlobalEditTask.ListIndex > 0)
                        {
                            Swap(&TaskView->List[GlobalEditTask.ListIndex - 1]->TaskID,
                                 &TaskView->List[GlobalEditTask.ListIndex]->TaskID);

                            Swap(&TaskView->List[GlobalEditTask.ListIndex], &TaskView->List[GlobalEditTask.ListIndex - 1]);
                            ListView_RedrawItems(TaskWindow, GlobalEditTask.ListIndex - 1, GlobalEditTask.ListIndex);
                            UpdateWindow(TaskWindow);
                            --GlobalEditTask.ListIndex;
                        }
                    }
                    OutputDebugStringA("UI_BUTTON_MOVEUP\n");
                } break;

                case UI_BUTTON_MOVEDOWN:
                {
                    if(GlobalEditTask.IsValid)
                    {
                        task_view *TaskView = TaskViews + GlobalEditTask.ListID;
                        HWND TaskWindow = TaskWindows[GlobalEditTask.ListID];

                        if(GlobalEditTask.ListIndex < (TaskView->ListCount - 1))
                        {
                            Swap(&TaskView->List[GlobalEditTask.ListIndex + 1]->TaskID,
                                 &TaskView->List[GlobalEditTask.ListIndex]->TaskID);

                            Swap(&TaskView->List[GlobalEditTask.ListIndex], &TaskView->List[GlobalEditTask.ListIndex + 1]);
                            ListView_RedrawItems(TaskWindow, GlobalEditTask.ListIndex, GlobalEditTask.ListIndex + 1);
                            UpdateWindow(TaskWindow);
                            ++GlobalEditTask.ListIndex;
                        }
                    }
                    OutputDebugStringA("UI_BUTTON_MOVEDOWN\n");
                } break;

                case UI_BUTTON_MOVETOP:
                {
                    if(GlobalEditTask.IsValid)
                    {
                        task_view *TaskView = TaskViews + GlobalEditTask.ListID;
                        HWND TaskWindow = TaskWindows[GlobalEditTask.ListID];

                        u32 TopTaskID = TaskView->List[0]->TaskID;
                        for(u32 Row = 0; Row < GlobalEditTask.ListIndex; ++Row)
                        {
                            TaskView->List[Row]->TaskID = TaskView->List[Row + 1]->TaskID;
                        }
                        GlobalEditTask.Task->TaskID = TopTaskID;

                        SortTaskViewByID(TaskView);
                        ListView_RedrawItems(TaskWindow, 0, GlobalEditTask.ListIndex);
                        UpdateWindow(TaskWindow);

                        GlobalEditTask.ListIndex = 0;
                    }
                    OutputDebugStringA("UI_BUTTON_MOVETOP\n");
                } break;

                case UI_BUTTON_MOVEBOTTOM:
                {
                    if(GlobalEditTask.IsValid)
                    {
                        task_view *TaskView = TaskViews + GlobalEditTask.ListID;
                        HWND TaskWindow = TaskWindows[GlobalEditTask.ListID];

                        u32 BottomTaskID = TaskView->List[TaskView->ListCount - 1]->TaskID;
                        u32 MaxRow = TaskView->ListCount - 1;
                        for(u32 Row = MaxRow; Row > GlobalEditTask.ListIndex; --Row)
                        {
                            TaskView->List[Row]->TaskID = TaskView->List[Row - 1]->TaskID;
                        }
                        GlobalEditTask.Task->TaskID = BottomTaskID;

                        SortTaskViewByID(TaskView);
                        ListView_RedrawItems(TaskWindow, GlobalEditTask.ListIndex, TaskView->ListCount);
                        UpdateWindow(TaskWindow);

                        GlobalEditTask.ListIndex = MaxRow;
                    }
                    OutputDebugStringA("UI_BUTTON_MOVEBOTTOM\n");
                } break;

                case APP_MENU_FILE_SAVE:
                {
                    char FileNameBuffer[MAX_PATH] = {0};
                    OPENFILENAMEA OpenFileName = {0};
                    OpenFileName.lStructSize = sizeof(OPENFILENAME);
                    OpenFileName.lpstrFilter = "Data files\0*.dat\0\0";
                    OpenFileName.lpstrFile = FileNameBuffer;
                    OpenFileName.nMaxFile = MAX_PATH;
                    OpenFileName.lpstrDefExt = "dat";
                    OpenFileName.Flags = OFN_DONTADDTORECENT|OFN_NONETWORKBUTTON|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
                    if(GetSaveFileNameA(&OpenFileName))
                    {
                        if(OpenFileName.lpstrFile[0])
                        {
                            WriteTaskListToFile(GlobalTaskList, OpenFileName.lpstrFile);

                            CopyNullTerminatedString(GlobalSaveFileName, OpenFileName.lpstrFile);
                            OutputDebugStringA("APP_MENU_FILE_SAVE\n");
                        }
                    }
                } break;

                case APP_MENU_FILE_LOAD:
                {
                    char FileNameBuffer[MAX_PATH] = {0};
                    OPENFILENAMEA OpenFileName = {0};
                    OpenFileName.lStructSize = sizeof(OPENFILENAME);
                    OpenFileName.lpstrFilter = "Data files\0*.dat\0\0";
                    OpenFileName.lpstrFile = FileNameBuffer;
                    OpenFileName.nMaxFile = MAX_PATH;
                    OpenFileName.lpstrDefExt = "dat";
                    OpenFileName.Flags = OFN_DONTADDTORECENT|OFN_NONETWORKBUTTON|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
                    if(GetOpenFileNameA(&OpenFileName))
                    {
                        if(OpenFileName.lpstrFile[0])
                        {
                            EndEditingTask(&GlobalEditTask);

                            for(u32 ListIndex = 0; ListIndex < TaskWindow_Count; ++ListIndex)
                            {
                                task_view *TaskView = TaskViews + ListIndex;
                                HWND TaskWindow = TaskWindows[ListIndex];
                                ListView_SetItemCount(TaskWindow, TaskView->ListCount);
                                ListView_RedrawItems(TaskWindow, 0, TaskView->ListCount);
                                UpdateWindow(TaskWindow);
                            }

                            PurgeTaskList(GlobalTaskList);
                            ResetNextTaskID();
                            ReadTaskListFromFile(Arena, GlobalTaskList, OpenFileName.lpstrFile);
                            PopulateViewsFromTaskList(GlobalTaskList);

                            CopyNullTerminatedString(GlobalSaveFileName, OpenFileName.lpstrFile);
                            OutputDebugStringA("\nAPP_MENU_FILE_LOAD\n");
                        }
                    }
                } break;

                case APP_MENU_FILE_EXIT:
                {
                    OutputDebugStringA("APP_MENU_FILE_EXIT\n");
                    PostQuitMessage(0);
                } break;
            }
        } break;

        case UI_TRAY_COMMAND:
        {
            if((LParam == WM_RBUTTONUP) ||
               (LParam == WM_LBUTTONUP))
            {
                HMENU TrayMenu = CreatePopupMenu();
                MENUINFO TrayMenuInfo = {0};
                TrayMenuInfo.cbSize = sizeof(MENUINFO);
                TrayMenuInfo.dwStyle = MNS_NOTIFYBYPOS;
                SetMenuInfo(TrayMenu, &TrayMenuInfo);
                AppendMenu(TrayMenu, MF_STRING, UI_TRAY_COMMAND_SHOW, "Show");
                AppendMenu(TrayMenu, MF_STRING, UI_TRAY_COMMAND_EXIT, "Quit");

                POINT CursorPosition = {0};
                GetCursorPos(&CursorPosition);
                SetForegroundWindow(Window);

                s32 MenuCommand = TrackPopupMenu(TrayMenu, TPM_NONOTIFY|TPM_RETURNCMD,
                                                 CursorPosition.x, CursorPosition.y,
                                                 0, Window, NULL);

                if(MenuCommand == UI_TRAY_COMMAND_SHOW)
                {
                    ShowWindow(Window, SW_RESTORE);
                    SetForegroundWindow(Window);
                }
                else if(MenuCommand == UI_TRAY_COMMAND_EXIT)
                {
                    RemoveTrayIcon(&GlobalTrayIconHandle);
                    PostQuitMessage(0);
                }
            }
        } break;

        case WM_TIMER:
        {
            u32 TimerID = (u32)WParam;
            if(TimerID == TIMER_AUTOSAVE)
            {
                if(GlobalSaveFileName[0])
                {
                    WriteTaskListToFile(GlobalTaskList, GlobalSaveFileName);
                    OutputDebugString("AUTOSAVE\n");
                }
            }
        } break;

        case WM_SIZE:
        {
            u32 SizeRequested = (u32)WParam;
            if(SizeRequested == SIZE_MINIMIZED)
            {
                ShowWindow(Window, SW_HIDE);
                GlobalTrayIconHandle = ShowTrayIcon(Window);
            }
            else
            {
                if(SizeRequested == SIZE_RESTORED)
                {
                    RemoveTrayIcon(&GlobalTrayIconHandle);
                }

                RECT ClientRect = {0};
                GetClientRect(Window, &ClientRect);
                u32 Width = ClientRect.right - ClientRect.left;
                u32 Height = ClientRect.bottom - ClientRect.top;
                MoveWindow(GlobalStatusBarHandle, 0, 0, Width, Height, true);
            }
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

int WINAPI
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
    WNDCLASSEXA WindowClass = {0};
    WindowClass.cbSize = sizeof(WindowClass);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(1));
    //WindowClass.hbrBackground = HBRUSH(COLOR_SCROLLBAR + 1);
    WindowClass.hbrBackground = HBRUSH(COLOR_WINDOW + 0);
    WindowClass.lpfnWndProc = Win32Callback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "win32_twobytwo";

    if(!RegisterClassExA(&WindowClass))
    {
        MessageBox(0, "Failed to register window class.", "Error", MB_OK);
        return(-1);
    }

    HWND Window = CreateWindowExA(0,
                                  WindowClass.lpszClassName, "2x2 - Task Manager",
                                  WS_POPUPWINDOW|WS_CAPTION|WS_VISIBLE|WS_MINIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  1600, 780,
                                  0, 0, Instance, 0);

    if(!Window)
    {
        MessageBox(0, "Failed to create window.", "Error", MB_OK);
        return(-2);
    }

    HFONT FixedWidthFont = CreateFontA(14, 0, 0, 0, FW_REGULAR, 0, 0, 0, DEFAULT_CHARSET,
                                       OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       FIXED_PITCH, "Consolas");

    Win32CreateMenu(Window);

    // NOTE(rick): Create edit UI components
    {
        // NOTE(rick): Status bar
        GlobalStatusBarHandle = CreateWindowEx(0, STATUSCLASSNAME, 0, WS_CHILD | WS_VISIBLE,
                                               0, 0, 0, 0, Window, 0, Instance, 0);

        // NOTE(rick): Title
        GlobalStaticTitleWindow = CreateWindowEx(0, "static", 0, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                                 1180, 20, 380, 20, Window, 0, Instance, 0);
        SetWindowTextA(GlobalStaticTitleWindow, "Create a title for the task");

        GlobalEditTitleWindow = CreateWindowEx(0, "edit", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                                               1180, 40, 380, 20, Window, 0, Instance, 0);
        Edit_LimitText(GlobalEditTitleWindow, 64);

        // NOTE(rick): Description
        GlobalStaticDescriptionWindow = CreateWindowEx(0, "static", 0, WS_CHILD | WS_VISIBLE,
                                                       1180, 70, 380, 20, Window, 0, Instance, 0);
        SetWindowTextA(GlobalStaticDescriptionWindow, "Write a description for the task");

        GlobalEditDescriptionWindow = CreateWindowEx(0, "edit", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN | WS_TABSTOP,
                                                     1180, 90, 380, 150, Window, 0, Instance, 0);

        Edit_LimitText(GlobalEditDescriptionWindow, 512);

        // NOTE(rick): Due Date
        GlobalStaticDueDateWindow = CreateWindowEx(0, "static", 0, WS_CHILD | WS_VISIBLE,
                                                   1180, 250, 380, 20, Window, 0, Instance, 0);
        SetWindowTextA(GlobalStaticDueDateWindow, "Enter due date");

        GlobalEditDueDateWindow = CreateWindowEx(0, "edit", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
                                                 1180, 270, 380, 20, Window, 0, Instance, 0);
        Edit_LimitText(GlobalEditDueDateWindow, 32);

        // NOTE(rick): Task Type
        GlobalStaticTypeWindow = CreateWindowEx(0, "static", 0, WS_CHILD | WS_VISIBLE,
                                                1180, 300, 380, 20, Window, 0, Instance, 0);
        SetWindowTextA(GlobalStaticTypeWindow, "Choose task type");

        GlobalTypeUrgentWindow = CreateWindowEx(0, "button", "Urgent", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                                                1180, 320, 112, 20, Window, (HMENU)UI_BUTTON_URGENT, Instance, 0);
        GlobalTypeImportantWindow = CreateWindowEx(0, "button", "Important", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                                                   1312, 320, 112, 20, Window, (HMENU)UI_BUTTON_IMPORTANT, Instance, 0);

        // NOTE(rick): Status
        GlobalStaticStatusWindow = CreateWindowEx(0, "static", 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                  1180, 350, 380, 20, Window, 0, Instance, 0);
        SetWindowTextA(GlobalStaticStatusWindow, "Select task progress");

        GlobalComboBoxStatusWindow = CreateWindowEx(0, WC_COMBOBOX, 0,
                                                    WS_VSCROLL | WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_DISABLENOSCROLL,
                                                    1180, 375, 380, 80, Window, (HMENU)APP_UI_COMBOBOX_STATUS, Instance, 0);
        for(u32 LabelIndex = 0; LabelIndex < ArrayCount(GlobalStatusStrings) - 1; ++LabelIndex)
        {
            SendMessage(GlobalComboBoxStatusWindow, CB_ADDSTRING, 0, (LPARAM)GlobalStatusStrings[LabelIndex]); 
        }
        SendMessage(GlobalComboBoxStatusWindow, CB_SETCURSEL, 0, 0);

        // NOTE(rick): Button
        GlobalCancelFormButton = CreateWindowEx(0, "button", "Cancel", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                                1180, 420, 112, 30, Window, (HMENU)UI_BUTTON_CANCEL, Instance, 0);
        GlobalCreateNewFormButton = CreateWindowEx(0, "button", "Create New", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                                   1312, 420, 112, 30, Window, (HMENU)UI_BUTTON_CREATE, Instance, 0);
        GlobalSaveFormButton = CreateWindowEx(0, "button", "Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                              1444, 420, 113, 30, Window, (HMENU)UI_BUTTON_SAVEAS, Instance, 0);
        Button_Enable(GlobalSaveFormButton, false);

        GlobalStaticMoveWindow = CreateWindowEx(0, "static", 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                  1180, 470, 380, 20, Window, 0, Instance, 0);
        SetWindowTextA(GlobalStaticMoveWindow, "Change task order");
        GlobalMoveTaskUpButton = CreateWindowEx(0, "button", "Up", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                              1180, 490, 80, 30, Window, (HMENU)UI_BUTTON_MOVEUP, Instance, 0);
        Button_Enable(GlobalMoveTaskUpButton, false);
        GlobalMoveTaskDownButton = CreateWindowEx(0, "button", "Down", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                              1280, 490, 80, 30, Window, (HMENU)UI_BUTTON_MOVEDOWN, Instance, 0);
        Button_Enable(GlobalMoveTaskDownButton, false);
        GlobalMoveTaskTopButton = CreateWindowEx(0, "button", "Top", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                              1380, 490, 80, 30, Window, (HMENU)UI_BUTTON_MOVETOP, Instance, 0);
        Button_Enable(GlobalMoveTaskTopButton, false);
        GlobalMoveTaskBottomButton = CreateWindowEx(0, "button", "Bottom", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                              1480, 490, 80, 30, Window, (HMENU)UI_BUTTON_MOVEBOTTOM, Instance, 0);
        Button_Enable(GlobalMoveTaskBottomButton, false);


        GlobalDeleteFormButton = CreateWindowEx(0, "button", "Delete", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                                1447, 650, 113, 30, Window, (HMENU)UI_BUTTON_DELETE, Instance, 0);
        Button_Enable(GlobalDeleteFormButton, false);


        PostMessageW(GlobalStatusBarHandle, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalStaticTitleWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalEditTitleWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalStaticDescriptionWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalEditDescriptionWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalStaticDueDateWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalEditDueDateWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalStaticStatusWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalComboBoxStatusWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalStaticTypeWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalTypeUrgentWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalTypeImportantWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalCancelFormButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalCreateNewFormButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalSaveFormButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalDeleteFormButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalMoveTaskUpButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalMoveTaskDownButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalMoveTaskTopButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalMoveTaskBottomButton, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
        PostMessageW(GlobalStaticMoveWindow, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
    }

    LVCOLUMNA ListViewDefaultColumn = {0};
    ListViewDefaultColumn.mask = LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;

    LVCOLUMNA ListViewColumns[ListViewColumn_Count] = {0};
    ListViewColumns[ListViewColumn_Title] = ListViewDefaultColumn;
    ListViewColumns[ListViewColumn_Title].pszText = "Name";
    ListViewColumns[ListViewColumn_Title].iSubItem = ListViewColumn_Title;
    ListViewColumns[ListViewColumn_Title].cx = 160;
    ListViewColumns[ListViewColumn_Title].fmt = LVCFMT_FIXED_WIDTH;
    ListViewColumns[ListViewColumn_Title].mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM|HDF_FIXEDWIDTH;
    ListViewColumns[ListViewColumn_Description] = ListViewDefaultColumn;
    ListViewColumns[ListViewColumn_Description].pszText = "Description";
    ListViewColumns[ListViewColumn_Description].iSubItem = ListViewColumn_Description;
    ListViewColumns[ListViewColumn_Description].cx = 240;
    ListViewColumns[ListViewColumn_Description].fmt = LVCFMT_FIXED_WIDTH;
    ListViewColumns[ListViewColumn_Description].mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
    ListViewColumns[ListViewColumn_DueDate] = ListViewDefaultColumn;
    ListViewColumns[ListViewColumn_DueDate].pszText = "DueDate";
    ListViewColumns[ListViewColumn_DueDate].iSubItem = ListViewColumn_DueDate;
    ListViewColumns[ListViewColumn_DueDate].cx = 70;
    ListViewColumns[ListViewColumn_DueDate].fmt = LVCFMT_FIXED_WIDTH;
    ListViewColumns[ListViewColumn_DueDate].mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
    ListViewColumns[ListViewColumn_Status] = ListViewDefaultColumn;
    ListViewColumns[ListViewColumn_Status].pszText = "Status";
    ListViewColumns[ListViewColumn_Status].iSubItem = ListViewColumn_Status;
    ListViewColumns[ListViewColumn_Status].cx = 90;
    ListViewColumns[ListViewColumn_Status].fmt = LVCFMT_FIXED_WIDTH;
    ListViewColumns[ListViewColumn_Status].mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;

    u64 TaskWindowID = TaskWindow_UrgentImportant;
    HWND *TaskWindowInsert = TaskWindows + TaskWindow_UrgentImportant;
    for(u32 ListViewY = 0; ListViewY < 0.5f*TaskWindow_Count; ++ListViewY)
    {
        for(u32 ListViewX = 0; ListViewX < 0.5f*TaskWindow_Count; ++ListViewX)
        {
            u32 ListViewWidth = 580;
            u32 ListViewHeight = 350;
            u32 XPos = ListViewX * ListViewWidth;
            u32 YPos = ListViewY * ListViewHeight;
            HWND ListView = CreateWindow(WC_LISTVIEW,
                                         0,
                                         LVS_OWNERDATA|LVS_REPORT|LVS_SINGLESEL|WS_VISIBLE|WS_CHILD|WS_EX_CLIENTEDGE|WS_VSCROLL|LVS_EX_FLATSB,
                                         XPos, YPos, ListViewWidth, ListViewHeight,
                                         Window, (HMENU)TaskWindowID++, Instance, 0);

            if(!ListView)
            {
                MessageBox(0, "Failed to create list view.", "Error", MB_OK);
                return(-3);
            }

            PostMessageW(ListView, WM_SETFONT, (WPARAM)FixedWidthFont, TRUE);
            ListView_SetExtendedListViewStyleEx(ListView, 0, LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|WS_VSCROLL);

            for(u32 ColumnIndex = 0; ColumnIndex < ListViewColumn_Count; ++ColumnIndex)
            {
                LVCOLUMNA *ColumnData = ListViewColumns + ColumnIndex;
                ListView_InsertColumn(ListView, ColumnIndex, ColumnData);
            }

            *TaskWindowInsert++ = ListView;
        }
    }

    GlobalTaskList = AllocateTaskList();
    Arena = AllocateArena();

#if 0
    {
        // NOTE(rick): Create random tasks
        for(int i = 0; i < 500; ++i)
        {
            task *Task = MakeRandomTask(Arena);
            TaskListInsert(GlobalTaskList, Task);
        }
    }
#endif

    // NOTE(rick): Initialize task_views
    {
        for(u32 TaskViewIndex = 0; TaskViewIndex < TaskWindow_Count; ++TaskViewIndex)
        {
            task_view *View = TaskViews + TaskViewIndex;
            View->ListMaxCount = 256;
            View->ListCount = 0;
            View->List = (task **)malloc(sizeof(*View->List) * View->ListMaxCount);
        }
    }

    // NOTE(rick): Insert tasks into task_views
    {
        for(task_list *List = GlobalTaskList; List; List = List->Next)
        {
            for(u32 ListIndex = 0; ListIndex < List->TaskCount; ++ListIndex)
            {
                task *Task = List->Tasks[ListIndex];
                if(Task->TaskState < TaskState_Removed)
                {
                    task_view *View = GetTaskViewForTask(Task);
                    Assert(View != &TaskViews[TaskWindow_Null]);
                    AddTaskToTaskView(View, Task);
                }

                Task++;
            }

            if(List->Next == GlobalTaskList)
            {
                break;
            }
        }
    }

    // NOTE(rick): Setup listviews
    {
        for(u32 TaskViewIndex = 0; TaskViewIndex < TaskWindow_Count; ++TaskViewIndex)
        {
            task_view *TaskView = TaskViews + TaskViewIndex;
            HWND *TaskWindowHandle = TaskWindows + TaskViewIndex;
            ListView_SetItemCountEx(*TaskWindowHandle, TaskView->ListCount, LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        }
    }

    // NOTE(rick): Setup auto save timer
    u32 MillisecondsPerSecond = 1000;
    u32 SecondsPerMinute = 60;
    u32 AutoSaveMinutes = 5;
    u32 AutoSaveInterval = (AutoSaveMinutes * SecondsPerMinute * MillisecondsPerSecond);
    SetTimer(Window, TIMER_AUTOSAVE, AutoSaveInterval, 0);

    for(;;)
    {
        MSG Message = {0};
        BOOL MessageResult = GetMessage(&Message, 0, 0, 0);

        if(MessageResult <= 0)
        {
            break;
        }
        else
        {
            b32 WasDialogMessage = IsDialogMessage(Window, &Message);
            if(!WasDialogMessage)
            {
                switch(Message.message)
                {
                    default:
                    {
                        TranslateMessage(&Message);
                        DispatchMessage(&Message);
                    } break;
                }
            }
        }
    }

    return(0);
}
