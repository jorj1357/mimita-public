#pragma once

// Resident image decode entry points. Names are prefixed so the migration
// EXE can link the game-owned stb_image implementation without collisions.

extern "C" {
unsigned char* hostStbiLoad(const char* filename, int* width, int* height,
                            int* channels, int desiredChannels);
const char* hostStbiFailureReason();
void hostStbiImageFree(void* pixels);
}
