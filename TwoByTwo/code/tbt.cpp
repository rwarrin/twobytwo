/**
 * TODO(rick)
 *
 * - UI
 *
 * - Refactoring
 *
 * - Bugs
 *   - Definitely :)
 *
 **/

static u32 GlobalNextTaskID = 0;
static task *GlobalTaskFreeList = 0;
static u32 NextAvailableTaskListID = 0;

static void
ClearTask(task *Task)
{
    Assert(Task);

    Task->TaskState = TaskState_Removed;
    Task->TaskUrgency = TaskUrgency_None;
    Task->TaskImportance =  TaskImportance_None;

    Task->TitleLength = 0;
    ZeroSize(Task->Title, Task->TitleMaxLength*sizeof(*Task->Title));

    Task->DescriptionLength = 0;
    ZeroSize(Task->Description, Task->DescriptionMaxLength*sizeof(*Task->Description));

    Task->DueDateLength = 0;
    ZeroSize(Task->DueDate, Task->DueDateMaxLength*sizeof(*Task->DueDate));
}

static task *
MakeEmptyTask(memory_arena *Arena)
{
#define MAX_TITLE_LENGTH 64
#define MAX_DESCRIPTION_LENGTH 512
#define MAX_DUEDATE_LENGTH 32

    task *Task = 0;
    if(GlobalTaskFreeList)
    {
        Task = GlobalTaskFreeList;
        GlobalTaskFreeList = Task->Next;
    }
    else
    {
        Task = PushStruct(Arena, task);
        Task->TitleMaxLength = MAX_TITLE_LENGTH;
        Task->TitleLength = 0;
        Task->Title = PushArray(Arena, Task->TitleMaxLength, char);

        Task->DescriptionMaxLength = MAX_DESCRIPTION_LENGTH;
        Task->DescriptionLength = 0;
        Task->Description = PushArray(Arena, Task->DescriptionMaxLength, char);

        Task->DueDateMaxLength = MAX_DUEDATE_LENGTH;
        Task->DueDateLength = 0;
        Task->DueDate = PushArray(Arena, Task->DueDateMaxLength, char);
    }

    Task->Next = 0;
    Task->TaskState = TaskState_NotStarted;
    Task->TaskUrgency = TaskUrgency_None;
    Task->TaskImportance = TaskImportance_None;
    Task->TaskID = GlobalNextTaskID++;

    return(Task);
#undef MAX_TITLE_LENGTH
#undef MAX_DESCRIPTION_LENGTH
}

// NOTE(rick): This version of UpdateTask will take NULL Terminated strings and
// set field lengths from the input strings.
static void
UpdateTask(task *Task,
           task_state TaskState, task_urgency TaskUrgency, task_importance TaskImportance,
           char *Title, char *Description, char *DueDate)
{
    Assert(Task);

    Task->TaskState = TaskState;
    Task->TaskUrgency = TaskUrgency;
    Task->TaskImportance = TaskImportance;

    Task->TitleLength = CopyNullTerminatedString(Task->Title, Title);
    Assert(Task->TitleLength < Task->TitleMaxLength);

    Task->DescriptionLength = CopyNullTerminatedString(Task->Description, Description);
    Assert(Task->DescriptionLength < Task->DescriptionMaxLength);

    Task->DueDateLength = CopyNullTerminatedString(Task->DueDate, DueDate);
    Assert(Task->DueDateLength < Task->DueDateMaxLength);
}

static void
ResetNextTaskID(u32 NewValue = 0)
{
    GlobalNextTaskID = NewValue;
}

static task_list *
AllocateTaskList()
{
    task_list *Result = 0;

    Result = (task_list *)malloc(sizeof(*Result));
    Assert(Result);
    if(Result)
    {
        ZeroStruct(Result);
        Result->Next = Result;
        Result->Previous = Result;
    }

    return(Result);
}

static void
TaskListInsert(task_list *RootList, task *Task)
{
    task_list *TaskList = 0;
    for(task_list *List = RootList; List; List = List->Next)
    {
        if((List->TaskCount + 1) < ArrayCount(List->Tasks))
        {
            TaskList = List;
            break;
        }
        else if(List->Next == RootList)
        {
            break;
        }
    }

    if(!TaskList)
    {
        TaskList = AllocateTaskList();
        
        TaskList->TaskListID = NextAvailableTaskListID++;
        TaskList->TaskCount = 0;
        TaskList->Next = RootList->Next;
        TaskList->Previous = RootList;
        TaskList->Next->Previous = TaskList;
        RootList->Next = TaskList;
    }

    Assert(TaskList);

    Task->ParentTaskListID = TaskList->TaskListID;
    Task->TaskListIndex = TaskList->TaskCount;
    TaskList->Tasks[TaskList->TaskCount++] = Task;
}

static void
TaskListRemoveTask(task_list *RootList, task *Task)
{
    task_list *ParentList = 0;
    for(task_list *List = RootList; List; List = List->Next)
    {
        if(List->TaskListID == Task->ParentTaskListID)
        {
            ParentList = List;
            break;
        }
        else if(List->Next == RootList)
        {
            break;
        }
    }
    Assert(ParentList);

    if(ParentList)
    {
        if(Task == ParentList->Tasks[Task->TaskListIndex])
        {
            ClearTask(Task);
            Task->Next = GlobalTaskFreeList;
            GlobalTaskFreeList = Task;
        }
        else
        {
            InvalidCodePath;
        }
    }
}

static void
PurgeTaskList(task_list *RootList)
{
    for(task_list *TaskList = RootList; TaskList; TaskList = TaskList->Next)
    {
        for(u32 TaskIndex = 0; TaskIndex < TaskList->TaskCount; ++TaskIndex)
        {
            task *Task = TaskList->Tasks[TaskIndex];

            Task->TaskState = TaskState_Removed;
            Task->TaskUrgency = TaskUrgency_None;
            Task->TaskImportance = TaskImportance_None;
            Task->TaskID = 0;

            Task->TitleLength = 0;
            Task->Title[0] = 0;

            Task->DescriptionLength = 0;
            Task->Description[0] = 0;

            Task->DueDateLength = 0;
            Task->DueDate[0] = 0;

            Task->Next = GlobalTaskFreeList;
            GlobalTaskFreeList = Task;

            TaskList->Tasks[TaskIndex] = 0;
        }

        TaskList->TaskCount = 0;

        if(TaskList->Next == RootList)
        {
            break;
        }
    }
}

static void
ClearAndRemoveTaskFromTaskList(task_list *RootList, task *Task)
{
    TaskListRemoveTask(RootList, Task);
}

static void
WriteTaskListToFile(task_list *RootList, char *FileName)
{
    FILE *File = fopen(FileName, "wb");
    if(File)
    {
        fwrite(&GlobalNextTaskID, sizeof(GlobalNextTaskID), 1, File);
        for(task_list *TaskList = RootList; TaskList; TaskList = TaskList->Next)
        {
            for(u32 TaskIndex = 0; TaskIndex < TaskList->TaskCount; ++TaskIndex)
            {
                task *Task = TaskList->Tasks[TaskIndex];
                if(Task->TaskState < TaskState_Removed)
                {
                    fwrite(&Task->TaskState, sizeof(Task->TaskState), 1, File);
                    fwrite(&Task->TaskUrgency, sizeof(Task->TaskUrgency), 1, File);
                    fwrite(&Task->TaskImportance, sizeof(Task->TaskImportance), 1, File);
                    fwrite(&Task->TaskID, sizeof(Task->TaskID), 1, File);
                    fwrite(&Task->TitleLength, sizeof(Task->TitleLength), 1, File);
                    fwrite(Task->Title, sizeof(*Task->Title), Task->TitleLength, File);
                    fwrite(&Task->DescriptionLength, sizeof(Task->DescriptionLength), 1, File);
                    fwrite(Task->Description, sizeof(*Task->Description), Task->DescriptionLength, File);
                    fwrite(&Task->DueDateLength, sizeof(Task->DueDateLength), 1, File);
                    fwrite(Task->DueDate, sizeof(*Task->DueDate), Task->DueDateLength, File);
                }
            }

            if(TaskList->Next == RootList)
            {
                break;
            }
        }
        fclose(File);
    }
}

static void
ReadTaskListFromFile(memory_arena *Arena, task_list *RootList, char *FileName)
{
    FILE *File = fopen(FileName, "rb");
    if(File)
    {
        u32 FileSize = 0;
        u8 *FileContents = 0;

        fseek(File, 0, SEEK_END);
        FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);

        FileContents = (u8 *)malloc(sizeof(u8) * FileSize);
        Assert(FileContents);

        if(fread(FileContents, FileSize, 1, File))
        {
            u8 *ReadAt = FileContents;

            u32 NextTaskIDFromFile = *(u32 *)(ReadAt);
            ReadAt += sizeof(NextTaskIDFromFile);

            while((ReadAt - FileContents) < FileSize)
            {
                task *NewTask = MakeEmptyTask(Arena);

                NewTask->TaskState = (task_state)(*(task_state *)ReadAt);
                ReadAt += sizeof(task_state);

                NewTask->TaskUrgency = (task_urgency)(*(task_urgency *)ReadAt);
                ReadAt += sizeof(task_urgency);

                NewTask->TaskImportance = (task_importance)(*(task_importance *)ReadAt);
                ReadAt += sizeof(task_importance);

                NewTask->TaskID = (u32)(*(u32 *)ReadAt);
                ReadAt += sizeof(NewTask->TaskID);

                NewTask->TitleLength = (u32)(*(u32 *)ReadAt);
                ReadAt += sizeof(u32);

                Copy(NewTask->Title, NewTask->TitleLength, ReadAt);
                NewTask->Title[NewTask->TitleLength] = 0;
                ReadAt += NewTask->TitleLength;

                NewTask->DescriptionLength = (u32)(*(u32 *)ReadAt);
                ReadAt += sizeof(u32);

                Copy(NewTask->Description, NewTask->DescriptionLength, ReadAt);
                NewTask->Description[NewTask->DescriptionLength] = 0;
                ReadAt += NewTask->DescriptionLength;

                NewTask->DueDateLength = (u32)(*(u32 *)ReadAt);
                ReadAt += sizeof(u32);

                Copy(NewTask->DueDate, NewTask->DueDateLength, ReadAt);
                NewTask->DueDate[NewTask->DueDateLength] = 0;
                ReadAt += NewTask->DueDateLength;

                TaskListInsert(RootList, NewTask);
            }

            Assert((ReadAt - FileContents) == FileSize);
            Assert(GlobalNextTaskID <= NextTaskIDFromFile);
            GlobalNextTaskID = NextTaskIDFromFile;
        }

        free(FileContents);
        fclose(File);
    }
}

// NOTE(rick): Used only for generating random data for testing. This can be
// removed eventually.
inline s32
RandomBetween(s32 Min, s32 Max)
{
    s32 Result = (Min + (rand() % Max));
    return(Result);
}

static char CharacterTable[] = 
{
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    ' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_',
};

static task *
MakeRandomTask(memory_arena *Arena)
{
    task *Task = MakeEmptyTask(Arena);
    if(Task)
    {
        Task->TaskState = (task_state)RandomBetween(TaskState_NotStarted, TaskState_Count - 1);
        Task->TaskUrgency = (task_urgency)RandomBetween(TaskUrgency_Urgent, TaskUrgency_NotUrgent);
        Task->TaskImportance = (task_importance)RandomBetween(TaskImportance_Important, TaskImportance_NotImportant);
        Task->TitleLength = 8 + RandomBetween(0, 0.5f*Task->TitleMaxLength);
        Task->DescriptionLength = 8 + RandomBetween(0, 0.5f*Task->DescriptionMaxLength);
        Task->DueDateLength = 8 + RandomBetween(0, 0.5f*Task->DueDateMaxLength);

#if 0
        for(u32 i = 0; i < Task->TitleLength; i++)
        {
            Task->Title[i] = CharacterTable[RandomBetween(0, ArrayCount(CharacterTable))];
        }
        Task->Title[Task->TitleLength] = 0;
#else
        Task->TitleLength = snprintf(Task->Title, Task->TitleMaxLength, "%u", Task->TaskID);
#endif

        for(u32 i = 0; i < Task->DescriptionLength; i++)
        {
            Task->Description[i] = CharacterTable[RandomBetween(0, ArrayCount(CharacterTable))];
        }
        Task->Description[Task->DescriptionLength] = 0;

        for(u32 i = 0; i < Task->DueDateLength; i++)
        {
            Task->DueDate[i] = CharacterTable[RandomBetween(0, ArrayCount(CharacterTable))];
        }
        Task->DueDate[Task->DueDateLength] = 0;
    }

    return(Task);
}
// NOTE(rick): End test data generators.
