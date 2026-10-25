// Copyright Epic Games, Inc. All Rights Reserved.

#include "PuertsToolsCommands.h"

#define LOCTEXT_NAMESPACE "FPuertsToolsModule"

void FPuertsToolsCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "PuertsTools", "Bring up PuertsTools window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
