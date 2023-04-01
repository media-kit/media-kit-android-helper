// This file is a part of media_kit (https://github.com/alexmercerind/media_kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the LICENSE file.

package com.alexmercerind.mediakitandroidhelper;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.widget.TextView;

import com.alexmercerind.mediakitandroidhelper.databinding.ActivityMainBinding;

import java.util.Locale;

public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        TextView textView = binding.sampleText;

        final String obj = "Hello World!";

        final long ref = MediaKitAndroidHelper.newGlobalObjectRef(obj);
        MediaKitAndroidHelper.deleteGlobalObjectRef(ref);

        textView.setText(String.format(Locale.ENGLISH, "%d", ref));

        MediaKitAndroidHelper.setApplicationContext(getApplicationContext());
        MediaKitAndroidHelper.copyAssetToExternalFilesDir("video/bee.mp4");
    }
}
