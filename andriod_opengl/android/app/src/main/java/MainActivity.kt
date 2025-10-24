package magiceye.android

import android.app.NativeActivity
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.KeyEvent
import android.content.ContentResolver
import android.view.inputmethod.InputMethodManager
import androidx.core.content.FileProvider
import androidx.documentfile.provider.DocumentFile

class MainActivity : NativeActivity() {

    companion object {
        private const val REQ_OPEN_DOWNLOADS_TREE = 1337
        private const val REQ_OPEN_PICTURES_TREE = 1338
        private const val PREFS = "me_prefs"
        private const val KEY_DOWNLOADS_URI = "downloads_tree_uri"
        private const val KEY_PICTURES_URI = "pictures_tree_uri"
    }

    // --------- Soft keyboard & unicode ----------
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
        return try {
            if (Build.VERSION.SDK_INT >= 26) {
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

    // ---------------------- SAF: Pictures access ----------------------------

    fun hasPicturesAccess(): Boolean {
        return getPicturesTreeUri()?.isNotEmpty() == true
    }

    fun requestPicturesAccess() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        intent.addFlags(
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or
                Intent.FLAG_GRANT_READ_URI_PERMISSION or
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
        val initial = picturesInitialUri()
        if (initial != null) {
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, initial)
        }
        startActivityForResult(intent, REQ_OPEN_PICTURES_TREE)
    }

    fun getPicturesTreeUri(): String? {
        val sp = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        return sp.getString(KEY_PICTURES_URI, null)
    }

    private fun setPicturesTreeUri(uri: String) {
        val sp = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        sp.edit().putString(KEY_PICTURES_URI, uri).apply()
    }

    private fun picturesInitialUri(): Uri? {
        return try {
            if (Build.VERSION.SDK_INT >= 26) {
                DocumentsContract.buildDocumentUri(
                    "com.android.externalstorage.documents",
                    "primary:Pictures"
                )
            } else null
        } catch (_: Throwable) {
            null
        }
    }

    // ---------------------- Activity result ---------------------------------

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        val uri = data?.data
        if (uri != null && (requestCode == REQ_OPEN_DOWNLOADS_TREE || requestCode == REQ_OPEN_PICTURES_TREE)) {
            try {
                contentResolver.takePersistableUriPermission(
                    uri,
                    (Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
                )
                if (requestCode == REQ_OPEN_DOWNLOADS_TREE) {
                    setDownloadsTreeUri(uri.toString())
                } else if (requestCode == REQ_OPEN_PICTURES_TREE) {
                    setPicturesTreeUri(uri.toString())
                }
            } catch (_: Throwable) {
                // ignore
            }
        }
    }

    // ------------------------ SAF helpers -----------------------------------

    private fun docFileFromTreeOrDoc(uri: Uri): DocumentFile? {
        return try {
            when {
                DocumentsContract.isTreeUri(uri) -> {
                    DocumentFile.fromTreeUri(this, uri)
                }
                DocumentsContract.isDocumentUri(this, uri) -> {
                    val docId = DocumentsContract.getDocumentId(uri)
                    val tree = DocumentsContract.buildTreeDocumentUri(uri.authority ?: return null, docId)
                    DocumentFile.fromTreeUri(this, tree)
                }
                else -> {
                    DocumentFile.fromSingleUri(this, uri)
                }
            }
        } catch (_: Throwable) {
            null
        }
    }

    fun listChildren(treeUri: String, filters: Array<String>): Array<String> {
        val normFilters = filters.map { f ->
            val lower = f.lowercase()
            if (lower.startsWith(".")) lower else ".$lower"
        }
        val uri = Uri.parse(treeUri)
        val root = docFileFromTreeOrDoc(uri)
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
            val out = java.io.File(cacheDir, displayName)
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

    // ------------------------ Export & Share helpers -------------------------

    private fun copyPathToTree(treeUri: String, sourcePath: String, displayName: String, mime: String): String? {
        val uri = Uri.parse(treeUri)
        val root = docFileFromTreeOrDoc(uri) ?: return null
        if (!root.canWrite()) return null

        val finalName = kotlin.run {
            var base = displayName.substringBeforeLast('.', displayName)
            var ext = displayName.substringAfterLast('.', "")
            var candidate = displayName
            var i = 1
            fun exists(name: String): Boolean = try {
                root.listFiles().any { it.name == name }
            } catch (_: Throwable) { false }
            while (exists(candidate)) {
                candidate = if (ext.isEmpty()) "${base} ($i)" else "${base} ($i).${ext}"
                i++
            }
            candidate
        }

        val dest: DocumentFile = try {
            root.createFile(mime, finalName) ?: return null
        } catch (_: Throwable) {
            return null
        }

        return try {
            val out = contentResolver.openOutputStream(dest.uri, "w") ?: return null
            java.io.FileInputStream(sourcePath).use { input ->
                out.use { output ->
                    input.copyTo(output)
                }
            }
            dest.uri.toString()
        } catch (_: Throwable) {
            null
        }
    }

    fun copyCachePathToPictures(cachePath: String, displayName: String, mime: String): String? {
        val tree = getPicturesTreeUri() ?: return null
        return copyPathToTree(tree, cachePath, displayName, mime)
    }

    fun shareDocumentUri(uriString: String, mime: String, subject: String?) {
        val uri = Uri.parse(uriString)
        val intent = Intent(Intent.ACTION_SEND)
        intent.type = mime
        intent.putExtra(Intent.EXTRA_STREAM, uri)
        if (!subject.isNullOrEmpty()) {
            intent.putExtra(Intent.EXTRA_SUBJECT, subject)
        }
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        intent.clipData = ClipData.newUri(contentResolver, "Shared content", uri)
        startActivity(Intent.createChooser(intent, "Share via"))
    }

    // NEW: Share a single cache file path using FileProvider (no SAF copy needed)
    fun shareCacheFilePath(path: String, mime: String, subject: String?) {
        val file = java.io.File(path)
        val uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", file)
        val intent = Intent(Intent.ACTION_SEND)
        intent.type = mime
        intent.putExtra(Intent.EXTRA_STREAM, uri)
        if (!subject.isNullOrEmpty()) intent.putExtra(Intent.EXTRA_SUBJECT, subject)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        intent.clipData = ClipData.newUri(contentResolver, "Shared content", uri)
        startActivity(Intent.createChooser(intent, "Share via"))
    }

    // NEW: Share multiple cache file paths using FileProvider
    fun shareCacheFilePaths(paths: Array<String>, mime: String, subject: String?) {
        val uris = ArrayList<Uri>(paths.size)
        val clip = ClipData.newPlainText("", "")
        for (p in paths) {
            val file = java.io.File(p)
            val uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", file)
            uris.add(uri)
            clip.addItem(ClipData.Item(uri))
        }
        val intent = Intent(Intent.ACTION_SEND_MULTIPLE)
        intent.type = mime
        intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris)
        if (!subject.isNullOrEmpty()) intent.putExtra(Intent.EXTRA_SUBJECT, subject)
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        intent.clipData = clip
        startActivity(Intent.createChooser(intent, "Share via"))
    }
}