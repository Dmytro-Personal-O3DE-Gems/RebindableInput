
#pragma once

//! Marks a string literal as a localization key, e.g. LOCALIZATION_KEY("input.key.space").
//!
//! The same marker as in LocalizationContract (LocalizationKey.h), repeated here so that this gem does not depend
//! on any localization gem: the texts of this gem are translated by whoever shows them (see InputHintsContract::InputHint).
//! LocalizationGem's check_localization.py finds the keys of enabled gems by this marker and reports keys missing
//! in the game's en.csv.
//!
//! For the compiler it is just the string literal. The "" in front makes anything but a string literal fail to compile.
//! The definition is the same as LocalizationContract's, so the two never conflict.
#ifndef LOCALIZATION_KEY
#define LOCALIZATION_KEY(key) "" key
#endif
