package magiceye.android

import android.app.NativeActivity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.KeyEvent
import android.view.inputmethod.InputMethodManager
import androidx.documentfile.provider.DocumentFile

class MainActivity : NativeActivity() {

    companion object {
        private const val REQ_OPEN_DOWNLOADS_TREE = 1337
        private const val PREFS = "me_prefs"
        private const val KEY_DOWNLOADS_URI = "downloads_tree_uri"
    }

    // --------- Soft keyboard & unicode (as in your existing sample) ----------
    private val unicodeQueue: ArrayDeque<Int> = ArrayDeque()

    fun showSoftInput() {
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, 0)
    }

    fun pollUnicodeChar(): Int {
        return if (unicodeQueue.isEmpty()) 0 else unicodeQueue.removeFirst()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_DOWN) {
            val uc = event.unicodeChar
            if (uc != 0) {
                unicodeQueue.addLast(uc)
            }
        }
        return super.dispatchKeyEvent(event)
    }

    // ---------------------- SAF: Downloads access ----------------------------

    fun hasDownloadsAccess(): Boolean {
        return getDownloadsTreeUri()?.isNotEmpty() == true
    }

    fun requestDownloadsAccess() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        intent.addFlags(
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or
                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
        // Try to hint Downloads as initial
        val initial = downloadsInitialUri()
        if (initial != null) {
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, initial)
        }
        startActivityForResult(intent, REQ_OPEN_DOWNLOADS_TREE)
    }

    fun getDownloadsTreeUri(): String? {
        val sp = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        return sp.getString(KEY_DOWNLOADS_URI, null)
    }

    private fun setDownloadsTreeUri(uri: String) {
        val sp = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        sp.edit().putString(KEY_DOWNLOADS_URI, uri).apply()
    }

    private fun downloadsInitialUri(): Uri? {
        // Note: EXTRA_INITIAL_URI is a hint. It may not be honored on all devices.
        return try {
            if (Build.VERSION.SDK_INT >= 26) {
                // Downloads provider root
                DocumentsContract.buildDocumentUri(
                    "com.android.providers.downloads.documents",
                    "downloads"
                )
            } else {
                null
            }
        } catch (_: Throwable) {
            null
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQ_OPEN_DOWNLOADS_TREE) {
            val uri = data?.data
            if (uri != null) {
                try {
                    // Persist permission so we can access it later without user action
                    contentResolver.takePersistableUriPermission(
                        uri,
                        (Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
                    )
                    setDownloadsTreeUri(uri.toString())
                } catch (_: Throwable) {
                    // ignore
                }
            }
        }
    }

    // ------------------------ SAF listing/copy helpers -----------------------

    // Returns encoded entries: "D|name|uri" for directories, "F|name|uri" for files.
    fun listChildren(treeUri: String, filters: Array<String>): Array<String> {
        val normFilters = filters.map { f ->
            val lower = f.lowercase()
            if (lower.startsWith(".")) lower else ".$lower"
        }
        val uri = Uri.parse(treeUri)
        val root = DocumentFile.fromTreeUri(this, uri)
        if (root == null || !root.canRead()) return emptyArray()

        val out = ArrayList<String>()
        try {
            for (child in root.listFiles()) {
                val name = child.name ?: continue
                if (child.isDirectory) {
                    out.add("D|$name|${child.uri}")
                } else if (child.isFile) {
                    val ext = name.substringAfterLast('.', missingDelimiterValue = "").lowercase()
                    if (normFilters.isEmpty() || normFilters.contains(if (ext.isEmpty()) "" else ".$ext")) {
                        out.add("F|$name|${child.uri}")
                    }
                }
            }
        } catch (_: Throwable) {
        }
        return out.toTypedArray()
    }

    // Copies the given document to app's cache and returns the full filesystem path
    fun copyDocumentToCache(documentUri: String): String? {
        val uri = Uri.parse(documentUri)
        val df = DocumentFile.fromSingleUri(this, uri)
        val displayName = df?.name ?: "file"
        val inStream = try {
            contentResolver.openInputStream(uri)
        } catch (_: Throwable) {
            null
        } ?: return null

        val outFile = kotlin.run {
            // Keep original extension to help downstream loaders
            val out = java.io.File(cacheDir, displayName)
            // Ensure unique name
            var i = 1
            var candidate = out
            while (candidate.exists()) {
                val base = displayName.substringBeforeLast('.', displayName)
                val ext = displayName.substringAfterLast('.', "")
                val nn = if (ext.isEmpty()) "${base} ($i)" else "${base} ($i).${ext}"
                candidate = java.io.File(cacheDir, nn)
                i++
            }
            candidate
        }

        try {
            outFile.outputStream().use { o ->
                inStream.use { i ->
                    i.copyTo(o)
                }
            }
        } catch (_: Throwable) {
            return null
        }
        return outFile.absolutePath
    }
}
