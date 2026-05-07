// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "NeuralGIModuleEditorStyle.h"

class FNeuralGIModuleEditorCommands : public TCommands<FNeuralGIModuleEditorCommands>
{
public:

	FNeuralGIModuleEditorCommands()
		: TCommands<FNeuralGIModuleEditorCommands>(TEXT("NeuralGIModuleEditor"), NSLOCTEXT("Contexts", "NeuralGIModuleEditor", "NeuralGIModuleEditor Plugin"), NAME_None, FNeuralGIModuleEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
