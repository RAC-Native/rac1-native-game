package org.racnative.rac1;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class LauncherActivity extends Activity {
    private static final int REQ_IMPORT_ASSETS = 1001;
    private static final String PREFS_NAME = "rac1_launcher";
    private static final String KEY_DATA_STAMP = "data_stamp";

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private TextView statusView;
    private TextView pathView;
    private TextView progressText;
    private ProgressBar progressBar;
    private Button importButton;
    private Button retryButton;
    private Button launchButton;
    private volatile boolean busy = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
        prepareAndCheck(true);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        executor.shutdownNow();
    }

    private File getFilesRoot() {
        File ext = getExternalFilesDir(null);
        if (ext != null) return ext;
        return getFilesDir();
    }

    private File getAssetsRoot() {
        return new File(getFilesRoot(), "assets");
    }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(0xFF101218);
        scroll.setFillViewport(true);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(24);
        root.setPadding(pad, pad, pad, pad);
        root.setGravity(Gravity.CENTER_VERTICAL);
        scroll.addView(root);

        TextView title = new TextView(this);
        title.setText("Ratchet & Clank Native (Android Vulkan Port)");
        title.setTextColor(0xFFFFFFFF);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        root.addView(title);

        statusView = new TextView(this);
        statusView.setTextColor(0xFFD6DCE8);
        statusView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        statusView.setPadding(0, dp(12), 0, dp(8));
        root.addView(statusView);

        pathView = new TextView(this);
        pathView.setTextColor(0xFF8EC8FF);
        pathView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        pathView.setTypeface(Typeface.MONOSPACE);
        pathView.setTextIsSelectable(true);
        pathView.setBackgroundColor(0xFF191D28);
        pathView.setPadding(dp(12), dp(10), dp(12), dp(10));
        root.addView(pathView);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setIndeterminate(true);
        progressBar.setPadding(0, dp(14), 0, dp(4));
        progressBar.setVisibility(View.GONE);
        root.addView(progressBar);

        progressText = new TextView(this);
        progressText.setTextColor(0xFFA7B3C7);
        progressText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        progressText.setPadding(0, 0, 0, dp(12));
        root.addView(progressText);

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        root.addView(actions);

        importButton = new Button(this);
        importButton.setText("Import PC assets/ folder");
        importButton.setOnClickListener(v -> pickAssetsFolder());
        actions.addView(importButton);

        retryButton = new Button(this);
        retryButton.setText("Retry check");
        retryButton.setOnClickListener(v -> prepareAndCheck(true));
        actions.addView(retryButton);

        launchButton = new Button(this);
        launchButton.setText("Launch Game");
        launchButton.setVisibility(View.GONE);
        launchButton.setOnClickListener(v -> startGame());
        actions.addView(launchButton);

        setContentView(scroll);
    }

    private int dp(int v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()));
    }

    private void setBusy(boolean b) {
        busy = b;
        importButton.setEnabled(!b);
        retryButton.setEnabled(!b);
        launchButton.setEnabled(!b);
        progressBar.setVisibility(b ? View.VISIBLE : View.GONE);
    }

    private void prepareAndCheck(boolean autoLaunchIfReady) {
        if (busy) return;
        setBusy(true);
        statusView.setText("Preparing data files and verifying game assets...");
        progressBar.setIndeterminate(true);
        progressText.setText("");

        File root = getFilesRoot();
        File assets = getAssetsRoot();
        pathView.setText("Game assets directory:\n" + assets.getAbsolutePath());

        executor.execute(() -> {
            String err = null;
            try {
                ensureBundledDataExtracted(root);
            } catch (IOException e) {
                err = "Failed to extract bundled data/: " + e.getMessage();
            }
            boolean ready = (err == null) && hasValidAssets(assets);
            String finalErr = err;
            mainHandler.post(() -> {
                setBusy(false);
                if (finalErr != null) {
                    statusView.setText(finalErr);
                    launchButton.setVisibility(View.GONE);
                    return;
                }
                if (ready) {
                    statusView.setText("Game assets ready.");
                    progressText.setText("Starting Ratchet & Clank...");
                    launchButton.setVisibility(View.VISIBLE);
                    if (autoLaunchIfReady) {
                        startGame();
                    }
                } else {
                    statusView.setText(
                        "Extracted PC game assets are required before launching.\n\n" +
                        "Run ./setup.sh --iso <your.iso> on PC, then either:\n" +
                        "1. Tap \"Import PC assets/ folder\" below and select your copied assets/ folder, OR\n" +
                        "2. Copy assets/ (disc/, generated/, dumps/) via USB/ADB into the folder below and tap Retry."
                    );
                    launchButton.setVisibility(View.GONE);
                }
            });
        });
    }

    private boolean hasValidAssets(File assetsDir) {
        File layout = new File(assetsDir, "disc/layout.txt");
        File discFiles = new File(assetsDir, "disc/files");
        File mrp = new File(assetsDir, "generated/mrp");
        File pool = new File(assetsDir, "generated/pool");
        return layout.isFile() && discFiles.isDirectory() && mrp.isDirectory() && pool.isDirectory();
    }

    private void ensureBundledDataExtracted(File filesRoot) throws IOException {
        PackageInfo pi;
        long stamp = 1L;
        try {
            pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            stamp = pi.lastUpdateTime;
        } catch (Exception ignored) {
        }
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        File dataOut = new File(filesRoot, "data");
        if (prefs.getLong(KEY_DATA_STAMP, -1L) == stamp && new File(dataOut, "lights").isDirectory()) {
            return;
        }
        copyApkAssetTree("data", dataOut);
        prefs.edit().putLong(KEY_DATA_STAMP, stamp).apply();
    }

    private void copyApkAssetTree(String assetPath, File dst) throws IOException {
        AssetManager am = getAssets();
        String[] children = am.list(assetPath);
        if (children != null && children.length > 0) {
            if (!dst.exists() && !dst.mkdirs()) {
                throw new IOException("Cannot create dir: " + dst);
            }
            for (String child : children) {
                copyApkAssetTree(assetPath + "/" + child, new File(dst, child));
            }
        } else {
            File parent = dst.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                throw new IOException("Cannot create parent dir: " + parent);
            }
            try (InputStream in = am.open(assetPath); OutputStream out = new FileOutputStream(dst)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
            }
        }
    }

    private void pickAssetsFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivityForResult(intent, REQ_IMPORT_ASSETS);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_IMPORT_ASSETS && resultCode == RESULT_OK && data != null) {
            Uri treeUri = data.getData();
            if (treeUri != null) {
                importAssetsFromTree(treeUri);
            }
        }
    }

    private void importAssetsFromTree(Uri treeUri) {
        if (busy) return;
        setBusy(true);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        progressBar.setIndeterminate(true);
        statusView.setText("Importing assets (skipping raw iso/ and generated/src/ C++ sources)...");

        executor.execute(() -> {
            long[] stats = new long[2]; // [0]=files, [1]=bytes
            String err = null;
            try {
                ContentResolver cr = getContentResolver();
                String rootDocId = DocumentsContract.getTreeDocumentId(treeUri);
                // If the user picked the repo/bundle parent containing an "assets" subdir, step into it.
                String sourceDocId = rootDocId;
                if (!hasChildNamed(cr, treeUri, rootDocId, "disc")) {
                    String assetsChildId = findChildDirId(cr, treeUri, rootDocId, "assets");
                    if (assetsChildId != null) {
                        sourceDocId = assetsChildId;
                    }
                }
                copyDocumentTree(cr, treeUri, sourceDocId, "", getAssetsRoot(), stats);
            } catch (Exception e) {
                err = e.getMessage() != null ? e.getMessage() : e.toString();
            }

            boolean ok = (err == null) && hasValidAssets(getAssetsRoot());
            String finalErr = err;
            mainHandler.post(() -> {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                setBusy(false);
                if (finalErr != null) {
                    statusView.setText("Import error: " + finalErr);
                } else if (ok) {
                    statusView.setText("Imported " + stats[0] + " files (" + (stats[1] / (1024 * 1024)) + " MB).");
                    launchButton.setVisibility(View.VISIBLE);
                    startGame();
                } else {
                    statusView.setText("Folder copied (" + stats[0] + " files), but disc/layout.txt or generated/mrp/pool are still missing. Select the extracted PC assets/ folder.");
                }
            });
        });
    }

    private boolean hasChildNamed(ContentResolver cr, Uri treeUri, String parentDocId, String name) {
        return findChildDirId(cr, treeUri, parentDocId, name) != null;
    }

    private String findChildDirId(ContentResolver cr, Uri treeUri, String parentDocId, String targetName) {
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
        String[] proj = new String[]{
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try (Cursor c = cr.query(childrenUri, proj, null, null, null)) {
            if (c == null) return null;
            while (c.moveToNext()) {
                String docId = c.getString(0);
                String name = c.getString(1);
                String mime = c.getString(2);
                if (targetName.equals(name) && DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    return docId;
                }
            }
        }
        return null;
    }

    private void copyDocumentTree(ContentResolver cr, Uri treeUri, String docId, String relPath, File dstDir, long[] stats) throws IOException {
        if (!dstDir.exists() && !dstDir.mkdirs()) {
            throw new IOException("Cannot create directory: " + dstDir);
        }
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, docId);
        String[] proj = new String[]{
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE
        };
        byte[] buf = new byte[128 * 1024];
        try (Cursor c = cr.query(childrenUri, proj, null, null, null)) {
            if (c == null) return;
            while (c.moveToNext()) {
                String childId = c.getString(0);
                String name = c.getString(1);
                String mime = c.getString(2);
                long remoteSize = c.isNull(3) ? -1L : c.getLong(3);
                String childRel = relPath.isEmpty() ? name : (relPath + "/" + name);

                // Skip PC compile-only C++ sources and huge raw disc ISOs.
                if (childRel.equals("generated/src") || childRel.equals("iso")) {
                    continue;
                }

                File out = new File(dstDir, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    copyDocumentTree(cr, treeUri, childId, childRel, out, stats);
                } else {
                    if (remoteSize >= 0 && out.isFile() && out.length() == remoteSize) {
                        stats[0]++;
                        stats[1] += remoteSize;
                        continue;
                    }
                    Uri docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId);
                    try (InputStream in = cr.openInputStream(docUri); OutputStream fos = new FileOutputStream(out)) {
                        if (in == null) throw new IOException("Cannot open " + childRel);
                        int n;
                        while ((n = in.read(buf)) > 0) {
                            fos.write(buf, 0, n);
                            stats[1] += n;
                        }
                    }
                    stats[0]++;
                    if ((stats[0] % 25) == 0) {
                        long count = stats[0];
                        long mb = stats[1] / (1024 * 1024);
                        String current = childRel;
                        mainHandler.post(() -> progressText.setText("Copied " + count + " files (" + mb + " MB): " + current));
                    }
                }
            }
        }
    }

    private void startGame() {
        Intent i = new Intent(this, GameActivity.class);
        startActivity(i);
        finish();
    }
}
