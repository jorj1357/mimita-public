# Code Signing for Mimita Setup

Windows SmartScreen and Defender will flag unsigned installers as
"suspicious" because they are downloaded from the internet and have no
reputation. Code signing eliminates this.

## Recommended: OV (Organization Validation) Code Signing

Cheaper than EV, sufficient to remove most warnings once reputation is built.

| Vendor | Annual Cost | Notes |
|--------|------------|-------|
| DigiCert | ~$250/yr | Most widely trusted |
| Sectigo | ~$200/yr | Good value |
| Let's Encrypt | N/A | Does NOT do code signing |

## Better: EV (Extended Validation) Code Signing

~$300-500/yr. Establishes immediate trust with SmartScreen.

## How to sign

Install the cert, then:

```powershell
signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 ^
  "C:\important\mimita-priv-v8\installer\MimitaSetup-1.0.0.exe"
```

Verify:

```powershell
signtool verify /pa /all "C:\important\mimita-priv-v8\installer\MimitaSetup-1.0.0.exe"
```

## Defender Submission

After signing, submit to Microsoft:

https://www.microsoft.com/en-us/wdsi/filesubmission

Upload the signed installer, select "Incorrectly detected as malware".
Usually cleared within 24 hours.
