# VerseLink Windows

A Windows application that runs in the background and automatically replaces Bible verse references with the full verse text when you press a hotkey.

## Features

- **Background Operation**: Runs silently in background until activated
- **Smart Text Selection**: Works with most applications using UI Automation, edit controls, or clipboard fallback
- **Robust Bible Reference Parsing**: Supports various formats including:
  - Single verses: `John 3:16`
  - Verse ranges: `Romans 8:1-5`, `John 3:16 - 17`, `Romans 8:28-9:1`
  - Multiple verses: `John 3:16,18,20`
  - Chapter ranges: `John 1-2`
  - Book ranges: `Genesis - Exodus`
  - Chapter references: `Genesis 1`
  - Book abbreviations: `gen 1:1`, `jn 3:16`, etc.
- **Multiple Bible Versions**: Supports KJV, and can be extended for other versions
- **Configurable Hotkey**: Default is Ctrl+Alt+L
- **Configurable Verse Formatting**:
  - Include verse numbers
  - Add new lines between chapters and books
  - Place reference on first line
  - Dynamic Reference (shorten to Book Chapter:FirstVerse)
- **Custom System Tray Icon**: Set your own icon via settings
- **Real-time Settings Updates**: Changes apply immediately without restart
- **Comprehensive Logging**: Detailed logging with multiple levels
- **Thread-Safe**: Prevents multiple simultaneous operations
- **Graceful Shutdown**: Proper cleanup on exit

## Requirements

- Windows 10 or later
- Visual Studio 2019 or later (for building)
- .NET Framework 4.7.2 or later (for UI Automation)
- Bible XML files in the Bibles folder

## Installation

1. **Download or Build**:
   - Download the latest release from Releases, or
   - Build from source using Visual Studio

2. **Place Files**:
   ```
   VerseLinkWindows/
   ├── VerseLinkWindows.exe
   ├── config.json
   ├── verselink.log (created automatically)
   └── Bibles/
       ├── KJV.xml
       ├── NASB.xml
       └── ESV.xml
   ```

3. **Run**:
   - Double-click `VerseLinkWindows.exe`
   - The application will start in the background
   - Check `verselink.log` for startup messages

## Usage

1. **Select Text**: Highlight a Bible verse reference in any application
   - Examples: `John 3:16`, `Romans 8:1-5`, `gen 1:1`

2. **Press Hotkey**: Use Ctrl+Alt+L (default)

3. **Text Replacement**: The selected text will be replaced with formatted verse text. Examples:
   - Default: `John 3:16 For God so loved the world...`
   - With verse numbers: `16 For God so loved... 17 For God did not...`
   - Reference on first line: `John 3:16-17\n16 For God so loved...`
   - Dynamic reference: `John 3:16\nFor God so loved... 17 For God did not...`

## Configuration

The application uses `config.json` for settings. Here are the main options:

```json
{
  "bibleVersion": "KJV.xml",
  "hotkeyModifiers": 3,
  "hotkeyVirtualKey": 76,
  "enableLogging": true,
  "debugMode": false,
  "includeReferenceInReplacement": true,
  "replacementFormat": "{reference} {text}",
  "logFilePath": "verselink.log",
  "includeVerseNumbers": false,
  "newLineBetweenChapters": false,
  "newLineBetweenBooks": false,
  "referenceOnFirstLine": false,
  "dynamicReference": false,
  "iconPath": ""
}
```

### Configuration Options

- **bibleVersion**: XML file containing Bible text (must be in Bibles folder)
- **hotkeyModifiers**: 3 = Ctrl+Alt, 2 = Ctrl, 1 = Alt
- **hotkeyVirtualKey**: 76 = 'L' key (ASCII code)
- **enableLogging**: Enable/disable logging
- **debugMode**: Show console for debugging
- **includeReferenceInReplacement**: Include reference in replacement text
- **replacementFormat**: Format string (use `{reference}` and `{text}` placeholders)
- **includeVerseNumbers**: Prefix each verse with its number
- **newLineBetweenChapters**: Insert line break between chapters
- **newLineBetweenBooks**: Insert line break between books
- **referenceOnFirstLine**: Place reference on first line, verse text below
- **dynamicReference**: Shorten reference to Book Chapter:FirstVerse only
- **iconPath**: Path to custom system tray icon (.ico/.exe)

## Supported Reference Formats

The application can parse various Bible reference formats:

### Single Verses
- `John 3:16`
- `jn 3:16`
- `John 3:16`

### Verse Ranges
- `Romans 8:1-5`
- `Psalm 23:1-6`
- `John 3:16 - 17` (with spaces)
- `Romans 8:28-9:1` (cross-chapter)
- `Genesis - Exodus` (book range)

### Multiple Verses
- `John 3:16,18,20`
- `1 Peter 1:3,5,7`

### Chapters
- `Genesis 1`
- `Psalm 23`
- `John 1-2` (chapter range)

### Book Abbreviations
- Full names: `Genesis`, `Exodus`, `John`
- Short abbreviations: `gen`, `exo`, `jn`
- Medium abbreviations: `genesis`, `exodus`, `john`
- Numbered books: `1 samuel`, `2 cor`, `1 john`

## Bible Data Files

Place Bible XML files in the `Bibles/` folder. The XML structure should be:

```xml
<bible>
<b n="Genesis">
  <c n="1">
    <v n="1">In the beginning God created the heaven and the earth.</v>
    <v n="2">And the earth was without form, and void; and darkness was upon the face of the deep. And the Spirit of God moved upon the face of the waters.</v>
  </c>
</b>
```

## Troubleshooting

### Common Issues

1. **Hotkey Not Working**:
   - Check if another application is using the same hotkey
   - Try running as administrator
   - Check the log file for error messages

2. **Text Selection Not Working**:
   - Try different applications (Notepad, Word, browsers)
   - Ensure text is actually selected before pressing hotkey
   - Check log for "No text selected" messages

3. **Verse Not Found**:
   - Verify Bible XML file exists and is properly formatted
   - Check spelling of book name and chapter/verse numbers
   - Ensure the verse exists in the selected Bible version

4. **Custom Icon Not Showing**:
   - Ensure icon path is correct and file exists
   - Try using absolute path
   - Supported formats: .ico, .exe
   - Icon loads on startup; if not showing, check log for loading errors

5. **Application Won't Start**:
   - Check if required Windows services are running
   - Verify .NET Framework is installed
   - Run as administrator if needed

### Debug Mode

Enable debug mode in `config.json`:
```json
{
  "debugMode": true,
  "enableConsoleLogging": true
}
```

This will show a console window with detailed logging information.

### Log File

Check `verselink.log` for detailed error messages and operation status. Log levels include:
- `INFO`: General operation messages
- `WARNING`: Non-critical issues
- `ERROR`: Serious problems
- `DEBUG`: Detailed debugging info

## Building from Source

1. **Prerequisites**:
   - Visual Studio 2019 or later
   - Windows 10 SDK

2. **Steps**:
   - Open `VerseLinkWindows.sln` in Visual Studio
   - Ensure all required libraries are linked
   - Build in Release or Debug mode

3. **Dependencies**:
   - UI Automation (UIAutomation.h)
   - COM libraries
   - tinyxml2 for XML parsing

## Security Considerations

- The application requires UI Automation permissions
- It reads clipboard content temporarily
- It sends keyboard input (Ctrl+C, Ctrl+V)
- All operations are logged for transparency

## Performance

- Minimal CPU usage when idle
- Fast text retrieval from XML files
- Efficient memory management with RAII
- Thread-safe operations prevent conflicts

## Support

For issues and feature requests:
1. Check the log file for error details
2. Review this README for common solutions
3. Ensure configuration is correct
4. Test with different applications

## License

This project is provided as-is for educational and personal use.

---

**Note**: This application modifies text content in other applications. Always verify the replacement text is correct before using in important documents.
