#ifndef TWOBYTWO_H

enum task_urgency
{
    TaskUrgency_None,

    TaskUrgency_Urgent,
    TaskUrgency_NotUrgent,

    TaskUrgency_Count,
};

enum task_importance
{
    TaskImportance_None,

    TaskImportance_Important,
    TaskImportance_NotImportant,

    TaskImportance_Count,
};

enum task_state
{
    TaskState_NotStarted,
    TaskState_InProgress,
    TaskState_Complete,

    TaskState_Removed,

    TaskState_Count,
};

struct task
{
    task_state TaskState;

    task_urgency TaskUrgency;
    task_importance TaskImportance;

    u32 TitleMaxLength;
    u32 TitleLength;
    //wchar_t *Title;
    char *Title;

    u32 DescriptionMaxLength;
    u32 DescriptionLength;
    char *Description;
    //wchar_t *Description;

    u32 DueDateMaxLength;
    u32 DueDateLength;
    char *DueDate;
    //wchar_t *DueDate;

    u32 ParentTaskListID;
    u32 TaskListIndex;

    u32 TaskID;

    task *Next;
};

#define TASK_LIST_SIZE 73
struct task_list
{
    u32 TaskListID;

    u32 TaskCount;
    task *Tasks[TASK_LIST_SIZE];

    task_list *Next;
    task_list *Previous;

    s32 FreeTaskListCount;
    task *FreeTaskList;
};


#define TWOBYTWO_H
#endif
