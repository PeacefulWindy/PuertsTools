// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "PuertsToolsStyle.h"

class FPuertsToolsCommands : public TCommands<FPuertsToolsCommands>
{
public:

	FPuertsToolsCommands()
		: TCommands<FPuertsToolsCommands>(TEXT("PuertsTools"), NSLOCTEXT("Contexts", "PuertsTools", "PuertsTools Plugin"), NAME_None, FPuertsToolsStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};