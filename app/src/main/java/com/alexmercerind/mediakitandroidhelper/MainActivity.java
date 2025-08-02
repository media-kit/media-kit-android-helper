// This file is a part of media_kit (https://github.com/alexmercerind/media_kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the LICENSE file.

package com.alexmercerind.mediakitandroidhelper;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import com.alexmercerind.mediakitandroidhelper.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {
    private static final Object OBJECT = new Object();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final ActivityMainBinding binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        MediaKitAndroidHelper.setApplicationContextJava(getApplicationContext());

        final long ref = MediaKitAndroidHelper.newGlobalObjectRef(OBJECT);
        final String path = MediaKitAndroidHelper.copyAssetToFilesDir("video/bee.mp4");
        final int fd = MediaKitAndroidHelper.openFileDescriptorJava("file://" + path);

        binding.textView0.setText(String.valueOf(ref));
        binding.textView1.setText(String.valueOf(path));
        binding.textView2.setText(String.valueOf(fd));
    }
}
