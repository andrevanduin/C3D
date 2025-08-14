
#pragma once
#include <application.h>
#include <containers/dynamic_array.h>
#include <events/types.h>
#include <identifiers/uuid.h>

struct GameState
{
    C3D::DynamicArray<C3D::RegisteredEventCallback> registeredCallbacks;
};
