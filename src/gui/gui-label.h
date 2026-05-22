// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-label.h
// mar 8 2026
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * guiLabel(args)
 *
 * this file DOES:
 * - draw one text label
 *
 * this file DOES NOT:
 * - handle clicks
 * - decide menu logic
 */

#pragma once

void guiLabel(
    const char* text,
    float x,
    float y
);