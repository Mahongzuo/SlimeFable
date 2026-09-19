// Copyright Epic Games, Inc.All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_AUTOMATION_DRIVER

#include "Misc/AutomationTest.h"
#include "AutomationDriverTypeDefs.h"
#include "IAutomationDriver.h"
#include "IAutomationDriverModule.h"
#include "IDriverElement.h"
#include "LocateBy.h"

BEGIN_DEFINE_SPEC(FMenuStartEliminationSpec, "Lyra.MenuStartEliminationSpec",
                  EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAutomationDriverPtr Driver;

	void ClickButton(const FString& Text) const;
END_DEFINE_SPEC(FMenuStartEliminationSpec)

void FMenuStartEliminationSpec::ClickButton(const FString& Text) const
{
	const auto Button = Driver->FindElement(By::TextFilter::Contains(By::Path("<SCommonButton>"), Text));
	Driver->Wait(Until::ElementIsInteractable(Button, FWaitTimeout::InSeconds(60)));
	Button->Click();
}

void FMenuStartEliminationSpec::Define()
{
	BeforeEach([this]()
	{
		if (IAutomationDriverModule::Get().IsEnabled())
		{
			// Check if the Driver was left enabled by a previous test
			// Disable it in this case to force a reset
			IAutomationDriverModule::Get().Disable();
		}
		IAutomationDriverModule::Get().Enable();

		Driver = IAutomationDriverModule::Get().CreateDriver();
	});

	AfterEach([this]()
	{
		Driver.Reset(); /* remove reference to allow destruction */
		IAutomationDriverModule::Get().Disable();
	});

	Describe("Menu Start Elimination", [this]()
	{
		It("Should click buttons to Start Elimination", EAsyncExecution::ThreadPool, [this]()
		{
			ClickButton("Play Lyra");

			ClickButton("Start a Game");

			ClickButton("Elimination");
		});
	});
}

#endif
