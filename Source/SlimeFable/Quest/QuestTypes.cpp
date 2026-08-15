#include "Quest/QuestTypes.h"

const FQuestChapter* UDayQuestBook::FindChapter(FName ChapterId) const
{
	for (const FQuestChapter& Chapter : Chapters)
	{
		if (Chapter.ChapterId == ChapterId)
		{
			return &Chapter;
		}
	}
	return nullptr;
}

const FQuestMain* UDayQuestBook::FindMain(FName ChapterId, FName QuestId) const
{
	if (const FQuestChapter* Chapter = FindChapter(ChapterId))
	{
		for (const FQuestMain& Main : Chapter->MainQuests)
		{
			if (Main.QuestId == QuestId)
			{
				return &Main;
			}
		}
		for (const FQuestMain& Side : Chapter->SideQuests)
		{
			if (Side.QuestId == QuestId)
			{
				return &Side;
			}
		}
	}
	return nullptr;
}

const FQuestMain* UDayQuestBook::FindSide(FName ChapterId, FName QuestId) const
{
	if (const FQuestChapter* Chapter = FindChapter(ChapterId))
	{
		for (const FQuestMain& Side : Chapter->SideQuests)
		{
			if (Side.QuestId == QuestId)
			{
				return &Side;
			}
		}
	}
	return nullptr;
}

bool UDayQuestBook::IsSideQuest(FName ChapterId, FName QuestId) const
{
	return FindSide(ChapterId, QuestId) != nullptr;
}

const FQuestBranch* UDayQuestBook::FindBranch(FName ChapterId, FName QuestId, FName BranchId) const
{
	if (const FQuestMain* Main = FindMain(ChapterId, QuestId))
	{
		for (const FQuestBranch& Branch : Main->Branches)
		{
			if (Branch.BranchId == BranchId)
			{
				return &Branch;
			}
		}
	}
	return nullptr;
}

bool UDayQuestBook::GetFirstIncomplete(const TSet<FName>& CompletedChapters, int32& OutChapter, int32& OutMain, int32& OutBranch) const
{
	for (int32 ChapterIndex = 0; ChapterIndex < Chapters.Num(); ++ChapterIndex)
	{
		const FQuestChapter& Chapter = Chapters[ChapterIndex];
		if (CompletedChapters.Contains(Chapter.ChapterId))
		{
			continue;
		}
		if (Chapter.MainQuests.Num() == 0)
		{
			continue;
		}
		OutChapter = ChapterIndex;
		OutMain = 0;
		OutBranch = 0;
		return true;
	}
	return false;
}

UDayQuestBook* UDayQuestBook::MakeSlimeLabTestBook(UObject* Outer)
{
	UDayQuestBook* Book = NewObject<UDayQuestBook>(Outer, TEXT("DA_Quest_SlimeLab"));
	Book->DayId = FName(TEXT("Lab"));
	Book->bDoNotSave = true;

	FQuestChapter Chapter;
	Chapter.ChapterId = FName(TEXT("Lab"));
	Chapter.Title = FText::FromString(TEXT("实验室"));

	FQuestMain Main;
	Main.QuestId = FName(TEXT("Trial"));
	Main.Title = FText::FromString(TEXT("任务系统试炼"));

	FQuestBranch Collect;
	Collect.BranchId = FName(TEXT("Stones"));
	Collect.Title = FText::FromString(TEXT("收集试炼石"));
	Collect.Type = EQuestObjectiveType::Collect;
	Collect.RequiredCount = 2;

	FQuestBranch Talk;
	Talk.BranchId = FName(TEXT("Stele"));
	Talk.Title = FText::FromString(TEXT("和试炼石碑对话"));
	Talk.Type = EQuestObjectiveType::Talk;
	Talk.RequiredCount = 1;

	FQuestBranch Reach;
	Reach.BranchId = FName(TEXT("Gate"));
	Reach.Title = FText::FromString(TEXT("走进终点圈"));
	Reach.Type = EQuestObjectiveType::Reach;
	Reach.RequiredCount = 1;

	Main.Branches.Add(Collect);
	Main.Branches.Add(Talk);
	Main.Branches.Add(Reach);
	Chapter.MainQuests.Add(Main);
	Book->Chapters.Add(Chapter);
	return Book;
}

namespace
{
	FQuestBranch MakeBranch(const TCHAR* Id, const TCHAR* Title, EQuestObjectiveType Type, int32 Count)
	{
		FQuestBranch Branch;
		Branch.BranchId = FName(Id);
		Branch.Title = FText::FromString(Title);
		Branch.Type = Type;
		Branch.RequiredCount = Count;
		return Branch;
	}

	FQuestChapter MakeYearChapter(const TCHAR* Year, const TCHAR* Title, const TCHAR* QuestId, const TCHAR* MainTitle, const TArray<FQuestBranch>& Branches, const TCHAR* NextYear, const TCHAR* SideId = nullptr, const TCHAR* SideTitle = nullptr)
	{
		FQuestMain Main;
		Main.QuestId = FName(QuestId);
		Main.Title = FText::FromString(MainTitle);
		Main.Branches = Branches;

		FQuestChapter Chapter;
		Chapter.ChapterId = FName(Year);
		Chapter.Title = FText::FromString(Title);
		Chapter.MainQuests.Add(Main);
		Chapter.NextChapterId = NextYear ? FName(NextYear) : NAME_None;
		if (SideId && SideTitle)
		{
			FQuestMain Side;
			Side.QuestId = FName(SideId);
			Side.Title = FText::FromString(SideTitle);
			Side.Branches.Add(MakeBranch(TEXT("Pick"), SideTitle, EQuestObjectiveType::Collect, 1));
			Chapter.SideQuests.Add(Side);
		}
		return Chapter;
	}
}

UDayQuestBook* UDayQuestBook::Make0815Book(UObject* Outer)
{
	UDayQuestBook* Book = NewObject<UDayQuestBook>(Outer, TEXT("DA_Quest_0815"));
	Book->DayId = FName(TEXT("0815"));
	Book->bDoNotSave = true;

	Book->Chapters.Add(MakeYearChapter(TEXT("1920"), TEXT("油墨与呐喊"), TEXT("Print"), TEXT("把创刊号送到工友手里"),
		{ MakeBranch(TEXT("Manuscripts"), TEXT("收集手稿"), EQuestObjectiveType::Collect, 3),
		  MakeBranch(TEXT("Editor"), TEXT("交给陈编开印"), EQuestObjectiveType::Talk, 1),
		  MakeBranch(TEXT("Workers"), TEXT("把创刊号交给工人"), EQuestObjectiveType::Talk, 3) },
		TEXT("1945"), TEXT("Souvenir1920"), TEXT("创刊号影印")));
	Book->Chapters.Add(MakeYearChapter(TEXT("1945"), TEXT("终战广播"), TEXT("Broadcast"), TEXT("打败守军，让胜利的声音传出去"),
		{ MakeBranch(TEXT("Samurai"), TEXT("打败武士"), EQuestObjectiveType::Defeat, 1),
		  MakeBranch(TEXT("Gunner"), TEXT("打败机枪手"), EQuestObjectiveType::Defeat, 1),
		  MakeBranch(TEXT("Emperor"), TEXT("打败天皇"), EQuestObjectiveType::Defeat, 1),
		  MakeBranch(TEXT("Play"), TEXT("按下播放"), EQuestObjectiveType::Talk, 1) },
		TEXT("1962"), TEXT("Souvenir1945"), TEXT("收音机")));
	Book->Chapters.Add(MakeYearChapter(TEXT("1962"), TEXT("日行一善"), TEXT("Help"), TEXT("日行一善"),
		{ MakeBranch(TEXT("Neighbors"), TEXT("帮助路人"), EQuestObjectiveType::Talk, 3),
		  MakeBranch(TEXT("Spirit"), TEXT("回到雷锋灵体处"), EQuestObjectiveType::Talk, 1) },
		TEXT("1985"), TEXT("Souvenir1962"), TEXT("缝补的手套")));
	Book->Chapters.Add(MakeYearChapter(TEXT("1985"), TEXT("点亮和平"), TEXT("Peace"), TEXT("点亮和平"),
		{ MakeBranch(TEXT("Flowers"), TEXT("收集和平白花"), EQuestObjectiveType::Collect, 6),
		  MakeBranch(TEXT("Altar"), TEXT("放到祭台"), EQuestObjectiveType::Talk, 1),
		  MakeBranch(TEXT("Bell"), TEXT("敲响和平钟"), EQuestObjectiveType::Talk, 1) },
		TEXT("2004"), TEXT("Souvenir1985"), TEXT("和平钟拓片")));
	Book->Chapters.Add(MakeYearChapter(TEXT("2004"), TEXT("欢迎回家"), TEXT("Cards"), TEXT("帮他们办完手续"),
		{ MakeBranch(TEXT("Papers"), TEXT("替申请人补齐材料"), EQuestObjectiveType::Collect, 2),
		  MakeBranch(TEXT("Officer"), TEXT("交给签证官盖章"), EQuestObjectiveType::Talk, 1),
		  MakeBranch(TEXT("Deliver"), TEXT("把绿卡交到申请人手上"), EQuestObjectiveType::Talk, 2) },
		TEXT("2021"), TEXT("Souvenir2004"), TEXT("绿卡样本")));
	Book->Chapters.Add(MakeYearChapter(TEXT("2021"), TEXT("最后的航班"), TEXT("Airlift"), TEXT("送上最后一班飞机"),
		{ MakeBranch(TEXT("Docs"), TEXT("收集护照和机票"), EQuestObjectiveType::Collect, 2),
		  MakeBranch(TEXT("Gate"), TEXT("护送到登机口"), EQuestObjectiveType::Reach, 1),
		  MakeBranch(TEXT("Board"), TEXT("登机"), EQuestObjectiveType::Talk, 1) },
		TEXT("2026"), TEXT("Souvenir2021"), TEXT("登机牌")));
	Book->Chapters.Add(MakeYearChapter(TEXT("2026"), TEXT("今日之笔"), TEXT("Today"), TEXT("写下今天"),
		{ MakeBranch(TEXT("Write"), TEXT("在史书上留言"), EQuestObjectiveType::Talk, 1) },
		nullptr));
	return Book;
}
