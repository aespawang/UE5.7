// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralGIModuleEditorCommands.h"

#define LOCTEXT_NAMESPACE "FNeuralGIModuleEditorModule"

void FNeuralGIModuleEditorCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "NeuralGIModuleEditor", "Execute NeuralGIModuleEditor action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
