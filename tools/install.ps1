<#
AfroSamuraiRecomp one-click installer (Windows 10/11 x64).

Asks for an install folder and your Afro Samurai (Europe or USA) ISO, then downloads a
portable toolchain from official sources into <root>\tools (no admin rights, no
PATH/registry changes, no Visual Studio installer), gets the ReXGlue SDK, clones
this repository, extracts your disc and builds afrorecomp.exe.

Safe to re-run: finished steps are remembered in <root>\.install and skipped.
Nothing is ever uploaded. Log: <root>\install.log

Usage (normally just double-click install.bat):
  powershell -ExecutionPolicy Bypass -File install.ps1 [options]
Options (all optional; anything missing is asked interactively):
  -InstallDir <dir>      install folder (alias -Root); skips the folder picker
  -Iso <file>            your Afro Samurai (Europe or USA) ISO
  -AcceptMsvcLicense     accept the Microsoft Visual Studio Build Tools license
                         without the interactive prompt
  -Shortcut yes|no       create desktop + Start menu shortcuts
  -Ref <branch|tag>      repository revision to build (default: main)
  -RepoUrl <url|path>    repository to clone (default: the GitHub repo)
  -LocalTree <dir>       copy this prepared source tree instead of cloning.
                         The graphical setup uses this for private 0.3.4.
                         It does not fetch public main.
  -NonInteractive        fail instead of prompting. Requires the other answers.
  -CleanupAfterInstall   at the end, delete everything not needed to play (no question)
  -KeepBuildFiles        at the end, keep the toolchain and build files (no question)
  -Language en|es|fr|it  installer + launcher/game language (default: asked, Windows language preselected)
Advanced:
  -GameDataDir <dir>     use an already extracted disc folder (must contain
                         default.xex) instead of an ISO; it is copied into
                         <root>\gamedata
#>
param(
  [Alias('Root')][string]$InstallDir, [string]$Iso, [switch]$AcceptMsvcLicense,
  [ValidateSet('', 'yes', 'no')][string]$Shortcut = '',
  [string]$Ref = 'main',
  [string]$RepoUrl = 'https://github.com/LomoXSoft/AfroSamuraiRecomp.git',
  [string]$GameDataDir,
  [string]$LocalTree = '',
  [switch]$NonInteractive,
  [switch]$Update,
  [switch]$CleanupAfterInstall, [switch]$KeepBuildFiles,
  [ValidateSet('', 'en', 'es', 'fr', 'it')][string]$Language = ''
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'Continue'
$hereLog = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
. (Join-Path $hereLog 'logwrite.ps1')
$script:DLSHARE = 0.9   # share of a download+extract step spent downloading
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$ISO_SHA1 = '29394DB1854DFC59C767D768C8D35E0A08ACB73D'
# Graphical setup for the private 0.3.4 tree passes -LocalTree and checks this.
$LOCAL_TREE_VERSION = '0.3.4'
$NEED_GB = 20
# Pinned downloads (official sources) with SHA-256.
$DL = @{
  mingit = @{ url = 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.4/MinGit-2.56.0.4-64-bit.zip'; sha = 'e3bf78351fe777fba47ddee5950082f8a98bb0f02604fdc7ba3ee2cb68ffa877'; file = 'MinGit-2.56.0.4-64-bit.zip' }
  cmake  = @{ url = 'https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-windows-x86_64.zip'; sha = '326c826486d46a23cf9d5a1c9ee3a567d427bdd02c4a34071937a2a675cff004'; file = 'cmake-4.4.5-windows-x86_64.zip' }
  ninja  = @{ url = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'; sha = '07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65'; file = 'ninja-win-1.13.2.zip' }
  llvm   = @{ url = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/clang%2Bllvm-20.1.8-x86_64-pc-windows-msvc.tar.xz'; sha = 'f229769f11d6a6edc8ada599c0cda964b7dee6ab1a08c6cf9dd7f513e85b107f'; file = 'clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz' }
  # Tukaani xz 5.8.1 Windows build. A zip, so it is unpacked without tar.
  # xz.exe needs liblzma.dll from the same bin_x86-64 folder.
  # SHA-256 of the asset whose signature xz-5.8.1-windows.zip.sig verifies
  # with Lasse Collin's key 3690C240CE51B4670D30AD1C38EE757D69184620.
  xz     = @{ url = 'https://github.com/tukaani-project/xz/releases/download/v5.8.1/xz-5.8.1-windows.zip'; sha = '62fdfde73d5c5d293bbb4a96211b29d09adbd56bc6736976e4c9fc9942ae3c67'; file = 'xz-5.8.1-windows.zip' }
  python = @{ url = 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip'; sha = '4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3'; file = 'python-3.12.10-embed-amd64.zip' }
  pycryptodome = @{ url = 'https://files.pythonhosted.org/packages/e1/52/7d0830ccb961c9e7224b88a60df1a4eab7fa6c5fd426d7b7c972988a4cac/pycryptodome-4.3.0-cp39-abi3-win_amd64.whl'; sha = '195c9d781905605b3b9de8567c47986a9d0ee513566b5150eb9400692465b610'; file = 'pycryptodome-4.3.0-cp39-abi3-win_amd64.zip' }
  # portable-msvc.py by Martins Mozeiko (public domain): downloads MSVC + Windows SDK
  # directly from Microsoft's official Visual Studio manifests.
  pmsvc  = @{ url = 'https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/raw/49b58850aa5e5c1e65f090d4651ee76607883e11/portable-msvc.py'; sha = '1791cccca594ff083ce8d3e0a139e6156d2c312e24332a909e608010f0230a51'; file = 'portable-msvc.py' }
  rexsdk = @{ url = 'https://github.com/rexglue/rexglue-sdk/releases/download/nightly-20261002-bd833a2a/rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip'; sha = '1e19d7ec9be0d9d7e1a0088a34a33b068d4dea520181e81318528ef3b37627e7'; file = 'rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip' }
}
# portable-msvc.py otherwise takes the newest; MSVC 14.52's C++ library needs Clang 22.
$MSVC_VERSION = '14.51'
$WINSDK_VERSION = '28000'
$REXGLUE_GIT = 'https://github.com/rexglue/rexglue-sdk.git'
$REXGLUE_COMMIT = 'bd833a2'

# ---------------------------------------------------------------- language ---
# Messages are kept in this file as plain ASCII (no accents) so they show the same
# in any console code page and in install.log. Index: 0 en, 1 es, 2 fr, 3 it.
$LANGS = @('en', 'es', 'fr', 'it')
$LANG_NAMES = @('English', 'Espanol', 'Francais', 'Italiano')
$LANG_XID = @(1, 5, 4, 6)   # XLanguage ids used by the launcher (afro_language.cfg)
$TR = @{
  'title' = @('AfroSamuraiRecomp installer', 'Instalador de AfroSamuraiRecomp', 'Installateur AfroSamuraiRecomp', 'Installatore AfroSamuraiRecomp')
  'intro' = @('Builds the game from YOUR OWN disc image. Nothing is uploaded anywhere.', 'Compila el juego a partir de TU PROPIA imagen del disco. No se sube nada a ningun sitio.', 'Compile le jeu a partir de TA PROPRE image du disque. Rien n''est envoye nulle part.', 'Compila il gioco dalla TUA immagine del disco. Nulla viene caricato da nessuna parte.')
  'yeskey' = @('y', 's', 'o', 's')
  'yn' = @('y/n', 's/n', 'o/n', 's/n')
  'pick_desc' = @('Choose WHERE to install AfroSamuraiRecomp (a new folder is created inside it)', 'Elige DONDE instalar AfroSamuraiRecomp (se crea una carpeta nueva dentro)', 'Choisis OU installer AfroSamuraiRecomp (un nouveau dossier est cree dedans)', 'Scegli DOVE installare AfroSamuraiRecomp (viene creata una nuova cartella al suo interno)')
  'ask_folder' = @('Folder to install into (a new folder is created inside it)', 'Carpeta donde instalar (se crea una carpeta nueva dentro)', 'Dossier d''installation (un nouveau dossier est cree dedans)', 'Cartella di installazione (viene creata una nuova cartella al suo interno)')
  'w_ascii' = @('the path contains non-ASCII characters (accents, etc.), which can break the toolchain', 'la ruta contiene caracteres no ASCII (acentos, etc.), que pueden romper las herramientas', 'le chemin contient des caracteres non ASCII (accents, etc.), qui peuvent casser les outils', 'il percorso contiene caratteri non ASCII (accenti, ecc.), che possono rompere gli strumenti')
  'w_long' = @('the path is long ({0} characters); the build creates deep folders, prefer something short like C:\Games', 'la ruta es larga ({0} caracteres); la compilacion crea carpetas profundas, mejor algo corto como C:\Juegos', 'le chemin est long ({0} caracteres) ; la compilation cree des dossiers profonds, prefere un chemin court comme C:\Jeux', 'il percorso e lungo ({0} caratteri); la compilazione crea cartelle profonde, meglio qualcosa di corto come C:\Giochi')
  'w_space' = @('only {0:n1} GB free on that drive, about {1} GB are needed', 'solo hay {0:n1} GB libres en esa unidad, hacen falta unos {1} GB', 'seulement {0:n1} Go libres sur ce disque, il faut environ {1} Go', 'solo {0:n1} GB liberi su quel disco, ne servono circa {1} GB')
  'warning' = @('WARNING', 'AVISO', 'ATTENTION', 'ATTENZIONE')
  'choose_folder' = @('Choose the folder to install into (a window opens; a new folder is created inside it)...', 'Elige la carpeta de instalacion (se abre una ventana; se crea una carpeta nueva dentro)...', 'Choisis le dossier d''installation (une fenetre s''ouvre ; un nouveau dossier est cree dedans)...', 'Scegli la cartella di installazione (si apre una finestra; viene creata una nuova cartella al suo interno)...')
  'no_folder' = @('No folder chosen. Type a folder to install into', 'No se ha elegido carpeta. Escribe una carpeta de instalacion', 'Aucun dossier choisi. Tape un dossier d''installation', 'Nessuna cartella scelta. Scrivi una cartella di installazione')
  'install_folder' = @('Install folder: {0}', 'Carpeta de instalacion: {0}', 'Dossier d''installation : {0}', 'Cartella di installazione: {0}')
  'install_there' = @('Install there?', 'Instalar ahi?', 'Installer ici ?', 'Installare qui?')
  'log_file' = @('log file: {0}', 'registro: {0}', 'journal : {0}', 'registro: {0}')
  'all_q' = @('All questions are asked now; after that the installation runs to the end on its own.', 'Todas las preguntas se hacen ahora; despues la instalacion sigue sola hasta el final.', 'Toutes les questions sont posees maintenant ; ensuite l''installation va seule jusqu''au bout.', 'Tutte le domande vengono fatte ora; poi l''installazione prosegue da sola fino alla fine.')
  'sel_iso' = @('Select your Afro Samurai ISO (Europe or USA)...', 'Selecciona tu ISO de Afro Samurai (Europa o EE. UU.)...', 'Selectionne ton ISO d''Afro Samurai (Europe ou USA)...', 'Seleziona la tua ISO di Afro Samurai (Europa o USA)...')
  'iso_path' = @('Path to your ISO', 'Ruta de tu ISO', 'Chemin de ton ISO', 'Percorso della tua ISO')
  'iso_nf' = @('ISO not found: {0}', 'No se encuentra la ISO: {0}', 'ISO introuvable : {0}', 'ISO non trovata: {0}')
  'no_xex' = @('{0} does not contain default.xex', '{0} no contiene default.xex', '{0} ne contient pas default.xex', '{0} non contiene default.xex')
  'need64' = @('A 64-bit Windows is required.', 'Hace falta Windows de 64 bits.', 'Un Windows 64 bits est necessaire.', 'Serve Windows a 64 bit.')
  'free' = @('free space on {0}: {1:n1} GB', 'espacio libre en {0}: {1:n1} GB', 'espace libre sur {0} : {1:n1} Go', 'spazio libero su {0}: {1:n1} GB')
  'nospace' = @('Not enough disk space: about {0} GB needed, {1:n1} GB free.', 'No hay espacio suficiente: hacen falta unos {0} GB, hay {1:n1} GB libres.', 'Pas assez d''espace disque : environ {0} Go necessaires, {1:n1} Go libres.', 'Spazio su disco insufficiente: servono circa {0} GB, liberi {1:n1} GB.')
  'nogpu' = @('WARNING: no hardware GPU found. The game needs a Direct3D 12 GPU with up-to-date drivers.', 'AVISO: no se ha encontrado GPU. El juego necesita una GPU Direct3D 12 con drivers actualizados.', 'ATTENTION : aucun GPU trouve. Le jeu a besoin d''un GPU Direct3D 12 avec des pilotes a jour.', 'ATTENZIONE: nessuna GPU trovata. Il gioco richiede una GPU Direct3D 12 con driver aggiornati.')
  'gpu' = @('GPU: {0} (driver {1})', 'GPU: {0} (driver {1})', 'GPU : {0} (pilote {1})', 'GPU: {0} (driver {1})')
  'gpu_need' = @('The game needs Direct3D 12 (feature level 11_0 or newer, i.e. most GPUs from 2015 on).', 'El juego necesita Direct3D 12 (feature level 11_0 o superior, casi todas las GPU desde 2015).', 'Le jeu a besoin de Direct3D 12 (feature level 11_0 ou plus, la plupart des GPU depuis 2015).', 'Il gioco richiede Direct3D 12 (feature level 11_0 o superiore, quasi tutte le GPU dal 2015).')
  'iso_sha_prog' = @('SHA-1 of the ISO', 'SHA-1 de la ISO', 'SHA-1 de l''ISO', 'SHA-1 della ISO')
  'iso_check' = @('Checking the ISO (SHA-1, takes a minute)...', 'Comprobando la ISO (SHA-1, tarda un minuto)...', 'Verification de l''ISO (SHA-1, une minute)...', 'Controllo della ISO (SHA-1, ci vuole un minuto)...')
  'iso_ok' = @('ISO matches Afro Samurai (Europe), Redump 17884.', 'La ISO coincide con Afro Samurai (Europa), Redump 17884.', 'L''ISO correspond a Afro Samurai (Europe), Redump 17884.', 'La ISO corrisponde ad Afro Samurai (Europa), Redump 17884.')
  'iso_bad' = @('WARNING: ISO SHA-1 is {0}, expected {1}.', 'AVISO: el SHA-1 de la ISO es {0}, se esperaba {1}.', 'ATTENTION : le SHA-1 de l''ISO est {0}, attendu {1}.', 'ATTENZIONE: lo SHA-1 della ISO e {0}, atteso {1}.')
  'iso_only' = @('The Europe and USA discs are supported. Only the Europe ISO has a known SHA-1; for any ISO the game executable is checked after extraction, and another version or a bad dump will NOT work.', 'Se admiten los discos de Europa y EE. UU. Solo la ISO europea tiene un SHA-1 conocido; en cualquier ISO se comprueba el ejecutable del juego tras extraerla, y otra version o un volcado malo NO funcionara.', 'Les disques Europe et USA sont pris en charge. Seule l''ISO Europe a un SHA-1 connu ; pour toute ISO, l''executable du jeu est verifie apres l''extraction, et une autre version ou un mauvais dump ne marchera PAS.', 'Sono supportati i dischi europeo e USA. Solo la ISO europea ha uno SHA-1 noto; per qualsiasi ISO l''eseguibile del gioco viene controllato dopo l''estrazione, e un''altra versione o un dump difettoso NON funzionera.')
  'xex_ok' = @('Game files: Afro Samurai ({0}).', 'Archivos del juego: Afro Samurai ({0}).', 'Fichiers du jeu : Afro Samurai ({0}).', 'File del gioco: Afro Samurai ({0}).')
  'xex_bad' = @('default.xex (SHA-1 {0}) is not from the Europe or USA disc, or the extraction is incomplete.', 'default.xex (SHA-1 {0}) no es del disco de Europa ni de EE. UU., o la extraccion esta incompleta.', 'default.xex (SHA-1 {0}) ne vient pas du disque Europe ou USA, ou l''extraction est incomplete.', 'default.xex (SHA-1 {0}) non proviene dal disco europeo o USA, oppure l''estrazione e incompleta.')
  'iso_continue' = @('SHA-1 does not match. Got {0}, expected {1}. Not verified. Continuing anyway.', 'El SHA-1 no coincide. Es {0}, se esperaba {1}. No verificada. Se continua igualmente.', 'Le SHA-1 ne correspond pas. Obtenu {0}, attendu {1}. Non verifiee. Poursuite malgre tout.', 'Lo SHA-1 non coincide. Ottenuto {0}, atteso {1}. Non verificata. Si continua comunque.')
  'cont' = @('Continue anyway?', 'Continuar de todos modos?', 'Continuer quand meme ?', 'Continuare comunque?')
  'iso_nomatch' = @('ISO does not match the supported version.', 'La ISO no coincide con la version soportada.', 'L''ISO ne correspond pas a la version prise en charge.', 'La ISO non corrisponde alla versione supportata.')
  'lic1' = @('The Microsoft C++ compiler and Windows SDK are downloaded from Microsoft''s official', 'El compilador C++ de Microsoft y el SDK de Windows se descargan de los servidores', 'Le compilateur C++ de Microsoft et le SDK Windows sont telecharges depuis les serveurs', 'Il compilatore C++ di Microsoft e il Windows SDK vengono scaricati dai server ufficiali')
  'lic2' = @('Visual Studio servers. They are covered by the Visual Studio Build Tools license:', 'oficiales de Visual Studio. Estan sujetos a la licencia de Visual Studio Build Tools:', 'officiels de Visual Studio. Ils sont soumis a la licence Visual Studio Build Tools :', 'di Visual Studio. Sono soggetti alla licenza di Visual Studio Build Tools:')
  'lic_q' = @('Do you accept the Visual Studio Build Tools license?', 'Aceptas la licencia de Visual Studio Build Tools?', 'Acceptes-tu la licence Visual Studio Build Tools ?', 'Accetti la licenza di Visual Studio Build Tools?')
  'lic_no' = @('The Microsoft license was not accepted; the compiler cannot be downloaded.', 'No se ha aceptado la licencia de Microsoft; no se puede descargar el compilador.', 'La licence Microsoft n''a pas ete acceptee ; le compilateur ne peut pas etre telecharge.', 'La licenza Microsoft non e stata accettata; il compilatore non puo essere scaricato.')
  'sc_q' = @('Create desktop and Start menu shortcuts when finished?', 'Crear accesos directos en el escritorio y el menu Inicio al terminar?', 'Creer des raccourcis sur le bureau et dans le menu Demarrer a la fin ?', 'Creare collegamenti sul desktop e nel menu Start alla fine?')
  'summary' = @('Summary', 'Resumen', 'Resume', 'Riepilogo')
  's_folder' = @('  install folder: {0}', '  carpeta:        {0}', '  dossier :       {0}', '  cartella:       {0}')
  's_data' = @('  game data:      {0}', '  datos del juego: {0}', '  donnees du jeu : {0}', '  dati del gioco: {0}')
  's_copied' = @('{0} (copied)', '{0} (copiado)', '{0} (copie)', '{0} (copiato)')
  's_extracted' = @('already extracted', 'ya extraidos', 'deja extraites', 'gia estratti')
  's_lic' = @('  Microsoft license: {0}', '  licencia Microsoft: {0}', '  licence Microsoft : {0}', '  licenza Microsoft: {0}')
  's_acc' = @('accepted', 'aceptada', 'acceptee', 'accettata')
  's_msvc' = @('MSVC already installed', 'MSVC ya instalado', 'MSVC deja installe', 'MSVC gia installato')
  's_sc' = @('  shortcuts:      {0}', '  accesos:        {0}', '  raccourcis :    {0}', '  collegamenti:   {0}')
  's_lang' = @('  language:       {0}', '  idioma:         {0}', '  langue :        {0}', '  lingua:         {0}')
  's_go' = @('  Starting now; no more questions. This takes about 20-30 minutes (up to about 1 hour on slower PCs).', '  Empezando; no hay mas preguntas. Tarda unos 20-30 minutos (hasta 1 hora en PCs lentos).', '  C''est parti ; plus de questions. Cela prend environ 20-30 minutes (jusqu''a 1 heure sur les PC lents).', '  Si parte; nessuna altra domanda. Ci vogliono circa 20-30 minuti (fino a 1 ora sui PC lenti).')
  'skip' = @('[skip] {0} (already done)', '[omitido] {0} (ya hecho)', '[ignore] {0} (deja fait)', '[saltato] {0} (gia fatto)')
  'done_in' = @('    done in {0:n0} s', '    hecho en {0:n0} s', '    fait en {0:n0} s', '    fatto in {0:n0} s')
  'step' = @('Step', 'Paso', 'Etape', 'Passo')
  'dl' = @('    downloading {0}', '    descargando {0}', '    telechargement {0}', '    download {0}')
  'dl_prog' = @('downloading {0}: {1:n0}/{2:n0} MB', 'descargando {0}: {1:n0}/{2:n0} MB', 'telechargement {0} : {1:n0}/{2:n0} Mo', 'download {0}: {1:n0}/{2:n0} MB')
  'sha_bad' = @('SHA-256 mismatch for {0} (got {1}, expected {2})', 'SHA-256 incorrecto para {0} (es {1}, se esperaba {2})', 'SHA-256 incorrect pour {0} ({1}, attendu {2})', 'SHA-256 errato per {0} ({1}, atteso {2})')
  'sha_ok' = @('    ok, {0:n1} MB, SHA-256 verified', '    ok, {0:n1} MB, SHA-256 verificado', '    ok, {0:n1} Mo, SHA-256 verifie', '    ok, {0:n1} MB, SHA-256 verificato')
  'extracting' = @('extracting {0}...', 'extrayendo {0}...', 'extraction {0}...', 'estrazione {0}...')
  'tar_fail' = @('    tar failed ({0}), falling back to .NET unzip', '    tar ha fallado ({0}), se usa el descompresor .NET', '    tar a echoue ({0}), utilisation de l''extraction .NET', '    tar non riuscito ({0}), uso l''estrazione .NET')
  'cant_extract' = @('cannot extract {0} (tar.exe missing or failed)', 'no se puede extraer {0} (falta tar.exe o ha fallado)', 'impossible d''extraire {0} (tar.exe absent ou en echec)', 'impossibile estrarre {0} (tar.exe mancante o non riuscito)')
  'compiling' = @('compiling {0}/{1}', 'compilando {0}/{1}', 'compilation {0}/{1}', 'compilazione {0}/{1}')
  'exit_code' = @('{0} exited with code {1}', '{0} ha terminado con el codigo {1}', '{0} s''est termine avec le code {1}', '{0} e terminato con il codice {1}')
  'missing' = @('{0} missing in {1}', 'falta {0} en {1}', '{0} absent de {1}', '{0} mancante in {1}')
  'missing1' = @('{0} missing', 'falta {0}', '{0} absent', '{0} mancante')
  'notpatched' = @('{0} is not the patched build (no {1})', '{0} no es la version parcheada (sin {1})', '{0} n''est pas la version patchee (pas de {1})', '{0} non e la versione con patch (manca {1})')
  'lic_not' = @('Microsoft license not accepted', 'Licencia de Microsoft no aceptada', 'Licence Microsoft non acceptee', 'Licenza Microsoft non accettata')
  'shallow' = @('    shallow submodule fetch failed, retrying with full history', '    la descarga reducida de submodulos ha fallado, reintentando con todo el historial', '    la recuperation reduite des sous-modules a echoue, nouvel essai avec tout l''historique', '    il download ridotto dei sottomoduli non e riuscito, riprovo con tutta la cronologia')
  'copying' = @('    copying {0} (advanced -GameDataDir option)', '    copiando {0} (opcion avanzada -GameDataDir)', '    copie de {0} (option avancee -GameDataDir)', '    copia di {0} (opzione avanzata -GameDataDir)')
  'robocopy' = @('robocopy failed ({0})', 'robocopy ha fallado ({0})', 'robocopy a echoue ({0})', 'robocopy non riuscito ({0})')
  'codegen' = @('code generation, a few minutes', 'generacion de codigo, unos minutos', 'generation de code, quelques minutes', 'generazione del codice, qualche minuto')
  'ok_file' = @('    ok: {0}', '    ok: {0}', '    ok : {0}', '    ok: {0}')
  'game_folder' = @('    game folder: {0}', '    carpeta del juego: {0}', '    dossier du jeu : {0}', '    cartella del gioco: {0}')
  'shortcut' = @('    shortcut: {0}', '    acceso directo: {0}', '    raccourci : {0}', '    collegamento: {0}')
  'all_done' = @('[100%] All steps finished.', '[100%] Todos los pasos terminados.', '[100%] Toutes les etapes sont terminees.', '[100%] Tutti i passi completati.')
  'inst_ok' = @('INSTALL OK in {0:n0} min.', 'INSTALACION OK en {0:n0} min.', 'INSTALLATION OK en {0:n0} min.', 'INSTALLAZIONE OK in {0:n0} min.')
  'play' = @('Play:  {0}   (hold Shift at start to show the launcher again)', 'Jugar: {0}   (manten Mayus al iniciar para volver a ver el lanzador)', 'Jouer : {0}   (maintiens Maj au demarrage pour revoir le lanceur)', 'Gioca: {0}   (tieni premuto Maiusc all''avvio per rivedere il launcher)')
  'saves' = @('Saves: {0}', 'Partidas: {0}', 'Sauvegardes : {0}', 'Salvataggi: {0}')
  'logs' = @('Log:   {0} (game), {1} (installer)', 'Registro: {0} (juego), {1} (instalador)', 'Journal : {0} (jeu), {1} (installateur)', 'Registro: {0} (gioco), {1} (installatore)')
  'cl_msg' = @('Installation finished.`n`nDelete the files that are only needed to build the game and keep just what is needed to play?`n`nThis frees about {0:n1} GB (toolchain, ReXGlue SDK and source, build files, downloads).`nKept: the game folder ({1}), the extracted game data, the logs. Your ISO and your saves are never touched.`n`nNote: updating or rebuilding later needs a full new install (everything is downloaded again).', 'Instalacion terminada.`n`nBorrar los archivos que solo hacen falta para compilar el juego y dejar solo lo necesario para jugar?`n`nLibera unos {0:n1} GB (herramientas, SDK y codigo de ReXGlue, archivos de compilacion, descargas).`nSe conserva: la carpeta del juego ({1}), los datos extraidos del juego, los registros. Tu ISO y tus partidas nunca se tocan.`n`nNota: para actualizar o recompilar despues hara falta una instalacion completa nueva (se descarga todo otra vez).', 'Installation terminee.`n`nSupprimer les fichiers qui ne servent qu''a compiler le jeu et garder seulement le necessaire pour jouer ?`n`nCela libere environ {0:n1} Go (outils, SDK et sources ReXGlue, fichiers de compilation, telechargements).`nConserves : le dossier du jeu ({1}), les donnees extraites du jeu, les journaux. Ton ISO et tes sauvegardes ne sont jamais touchees.`n`nNote : pour mettre a jour ou recompiler plus tard, il faudra une installation complete (tout est retelecharge).', 'Installazione completata.`n`nEliminare i file che servono solo per compilare il gioco e tenere solo il necessario per giocare?`n`nLibera circa {0:n1} GB (strumenti, SDK e sorgenti ReXGlue, file di compilazione, download).`nVengono tenuti: la cartella del gioco ({1}), i dati estratti del gioco, i registri. La tua ISO e i tuoi salvataggi non vengono mai toccati.`n`nNota: per aggiornare o ricompilare in seguito servira una nuova installazione completa (si riscarica tutto).')
  'cl_title' = @('AfroSamuraiRecomp - free disk space?', 'AfroSamuraiRecomp - liberar espacio?', 'AfroSamuraiRecomp - liberer de l''espace ?', 'AfroSamuraiRecomp - liberare spazio?')
  'cl_q' = @('Delete them?', 'Borrarlos?', 'Les supprimer ?', 'Eliminarli?')
  'cl_del' = @('Deleting build files ({0:n1} GB)...', 'Borrando archivos de compilacion ({0:n1} GB)...', 'Suppression des fichiers de compilation ({0:n1} Go)...', 'Eliminazione dei file di compilazione ({0:n1} GB)...')
  'deleted' = @('    deleted {0}', '    borrado {0}', '    supprime {0}', '    eliminato {0}')
  'cl_freed' = @('Freed about {0:n1} GB. To update later, run the installer again (full download).', 'Liberados unos {0:n1} GB. Para actualizar, ejecuta el instalador otra vez (descarga completa).', 'Environ {0:n1} Go liberes. Pour mettre a jour, relance l''installateur (telechargement complet).', 'Liberati circa {0:n1} GB. Per aggiornare, esegui di nuovo l''installatore (download completo).')
  'cl_kept' = @('Build files kept ({0:n1} GB in tools, sdk, rexglue-sdk, afrorecomp, downloads).', 'Archivos de compilacion conservados ({0:n1} GB en tools, sdk, rexglue-sdk, afrorecomp, downloads).', 'Fichiers de compilation conserves ({0:n1} Go dans tools, sdk, rexglue-sdk, afrorecomp, downloads).', 'File di compilazione mantenuti ({0:n1} GB in tools, sdk, rexglue-sdk, afrorecomp, downloads).')
  'failed' = @('INSTALL FAILED at step: {0}', 'LA INSTALACION HA FALLADO en el paso: {0}', 'ECHEC DE L''INSTALLATION a l''etape : {0}', 'INSTALLAZIONE NON RIUSCITA al passo: {0}')
  'error' = @('Error: {0}', 'Error: {0}', 'Erreur : {0}', 'Errore: {0}')
  'full_log' = @('Full log: {0}', 'Registro completo: {0}', 'Journal complet : {0}', 'Registro completo: {0}')
  'retry' = @('Fix the problem and run the installer again: finished steps are skipped.', 'Corrige el problema y ejecuta el instalador otra vez: los pasos terminados se omiten.', 'Corrige le probleme et relance l''installateur : les etapes terminees sont ignorees.', 'Risolvi il problema ed esegui di nuovo l''installatore: i passi completati vengono saltati.')
  'lang_cfg' = @('    launcher language preset: {0}', '    idioma del lanzador preconfigurado: {0}', '    langue du lanceur preconfiguree : {0}', '    lingua del launcher preimpostata: {0}')
  't_git' = @('MinGit (portable Git)', 'MinGit (Git portable)', 'MinGit (Git portable)', 'MinGit (Git portatile)')
  't_cmake' = @('CMake (portable)', 'CMake (portable)', 'CMake (portable)', 'CMake (portatile)')
  't_python' = @('Python 3.12 (embeddable) + pycryptodome', 'Python 3.12 (embebible) + pycryptodome', 'Python 3.12 (embarquable) + pycryptodome', 'Python 3.12 (incorporabile) + pycryptodome')
  't_llvm' = @('LLVM/Clang 20.1.8 (about 940 MB download)', 'LLVM/Clang 20.1.8 (unos 940 MB de descarga)', 'LLVM/Clang 20.1.8 (environ 940 Mo a telecharger)', 'LLVM/Clang 20.1.8 (circa 940 MB da scaricare)')
  't_msvc' = @('MSVC compiler + Windows SDK (portable, from Microsoft)', 'Compilador MSVC + Windows SDK (portable, de Microsoft)', 'Compilateur MSVC + Windows SDK (portable, de Microsoft)', 'Compilatore MSVC + Windows SDK (portatile, da Microsoft)')
  't_repo' = @('AfroSamuraiRecomp source ({0})', 'Codigo de AfroSamuraiRecomp ({0})', 'Sources d''AfroSamuraiRecomp ({0})', 'Sorgenti di AfroSamuraiRecomp ({0})')
  't_rexsrc' = @('ReXGlue source at {0}', 'Codigo de ReXGlue en {0}', 'Sources ReXGlue a {0}', 'Sorgenti ReXGlue a {0}')
  't_gamedata' = @('Game files', 'Archivos del juego', 'Fichiers du jeu', 'File del gioco')
  't_app' = @('Build the game', 'Compilar el juego', 'Compiler le jeu', 'Compilare il gioco')
  't_gpu' = @('Build patched GPU plugin and runtime', 'Compilar el plugin de GPU y el runtime parcheados', 'Compiler le plugin GPU et le runtime patches', 'Compilare plugin GPU e runtime con patch')
}
$script:LI = 0
function T($k) { $v = $TR[$k]; if (-not $v) { return $k }; $s = $v[$script:LI]; if ($args.Count) { $s = $s -f $args }; $s.Replace('`n', "`n") }

function Say($m, $c = 'Gray') {
  # The setup window reads this process's stdout. Write-Host is not flushed on
  # that pipe, so a long compile looked frozen while install.log kept growing.
  try {
    [Console]::Out.WriteLine([string]$m)
    [Console]::Out.Flush()
  } catch { Write-Host $m -ForegroundColor $c }
  Write-InstallLog ([string]$m)
}
function Ask($q, $def) { $a = Read-Host "$q [$def]"; if ([string]::IsNullOrWhiteSpace($a)) { $def } else { $a.Trim().Trim('"') } }
function YesNo($q, $def = 'y') {
  $yk = T 'yeskey'; if ($def -eq 'y') { $def = $yk }
  $a = (Ask "$q ($(T 'yn'))" $def).ToLower(); $a.StartsWith('y') -or $a.StartsWith($yk)
}
$script:STEP = 'start'; $script:BASE = 0
# Overall progress: each step weighted by its typical duration (minutes).
$PLAN = [ordered]@{ 'iso-hash' = 1; git = 1; cmake = 1; ninja = 0.3; python = 0.5; llvm = 8; msvc = 6; repo = 0.3; sdk = 1; 'rexglue-src' = 4; gamedata = 4; 'build-app' = 30; 'build-gpu' = 15; verify = 0.2 }
$TOTALW = ($PLAN.Values | Measure-Object -Sum).Sum
$script:DONEW = 0; $script:CUR = $null; $script:LASTLOGPCT = -10; $script:LASTDETAIL = ''; $script:LASTDRAW = [DateTime]::MinValue
$script:StepFrac = 0
function Prog([double]$frac, [string]$detail = '', [switch]$Force) {
  # A nested download must not pull this step's overall percent backwards.
  if ($frac -lt $script:StepFrac) { $frac = $script:StepFrac } else { $script:StepFrac = $frac }
  $w = if ($script:CUR) { $PLAN[$script:CUR] } else { 0 }
  $pct = [math]::Min(99, [math]::Floor(100 * ($script:DONEW + $w * [math]::Max([double]0, [math]::Min([double]1, $frac))) / $TOTALW))
  # The visible step number is the AFROSTEP plan, not the weighted PLAN
  # (that one counts iso-hash and made LLVM look like step 6 of 14).
  $phaseIndex = if ($script:PhaseKey) { [array]::IndexOf($script:PhasePlan, $script:PhaseKey) } else { -1 }
  if ($phaseIndex -ge 0) {
    $line = "[{0,3}%] {1} {2}/{3}: {4}" -f $pct, (T 'step'), ($phaseIndex + 1), $script:PhasePlan.Count, $script:STEP
  } else {
    $line = "[{0,3}%] {1}" -f $pct, $script:STEP
  }
  if ($detail) { $line += " ($detail)" }
  if (-not $Force -and ((Get-Date) - $script:LASTDRAW).TotalMilliseconds -lt 500) { return }
  $script:LASTDRAW = Get-Date
  try { $Host.UI.RawUI.WindowTitle = "AfroSamuraiRecomp installer $line" } catch { }
  Write-Progress -Activity (T 'title') -Status $line -PercentComplete $pct
  # The setup window does not read this overall percent. It reads AFROSTEP
  # lines, one bar per step that is actually running. This line is the log.
  $detailChanged = [bool]$detail -and ($detail -ne $script:LASTDETAIL)
  if ($Force -or $pct -ne $script:LASTLOGPCT -or $detailChanged) {
    $script:LASTLOGPCT = $pct
    $script:LASTDETAIL = $detail
    Say $line White
  }
}
function Finish($n) { $script:DONEW += $PLAN[$n]; $script:CUR = $null }
function Done($n) { Test-Path -LiteralPath (Join-Path $STAMPS "$n.done") }
function Mark($n) { New-Item -ItemType File -Force -Path (Join-Path $STAMPS "$n.done") | Out-Null }
# Steps the window will count. Same order as LogicalSetupSteps in afrosetup_cmd.h.
# A tool that is already present is not one of them.
$script:PhasePlan = @()
$script:PhaseKey = ''
$script:LastStepLine = ''
$script:LastBucket = ''
$script:CodegenLog = $false
$script:PhaseT0 = $null
$script:NinjaSteps = ''
function Set-PhasePlan {
  $script:PhasePlan = @()
  foreach ($n in @('git','cmake','ninja','python','llvm','msvc')) {
    if (-not (Done $n)) { $script:PhasePlan += $n }
  }
  $script:PhasePlan += 'repo'
  foreach ($n in @('sdk','rexglue-src','gamedata')) {
    if (-not (Done $n)) { $script:PhasePlan += $n }
  }
  $script:PhasePlan += @('codegen','compile','gpu','install')
}
function Emit-Step([string]$key, [int]$pct = -1, [long]$done = 0, [long]$total = 0, [string]$activity = '') {
  $i = [array]::IndexOf($script:PhasePlan, $key)
  if ($i -lt 0) { return }
  # A measured step emits when its integer percent changes, plus the first and
  # last unit. That is the real ratio, not a timer, and it does not write one
  # log line per file.
  if ($total -gt 0 -and $done -gt 0 -and $done -lt $total) {
    $bucket = "$key|$activity|" + [int][math]::Floor(100.0 * $done / $total)
    if ($bucket -eq $script:LastBucket) { return }
    $script:LastBucket = $bucket
  } elseif ($script:PhaseKey -ne $key) {
    $script:LastBucket = ''
  }
  $suffix = if ($activity) { " $activity" } else { '' }
  $line = "AFROSTEP $($i + 1)/$($script:PhasePlan.Count) $key $pct $done $total$suffix"
  if ($line -eq $script:LastStepLine) { return }
  $script:LastStepLine = $line
  $script:PhaseKey = $key
  try {
    [Console]::Out.WriteLine($line)
    [Console]::Out.Flush()
  } catch { Write-Host $line }
  Write-InstallLog $line
}
function Same-Sha256([string]$got, [string]$expected) {
  # PowerShell's -eq ignores case, but the check is written out so a real
  # one-digit difference still fails and uppercase from Get-FileHash matches.
  if (-not $got -or -not $expected) { return $false }
  $g = ($got -replace '\s', '').ToUpperInvariant()
  $e = ($expected -replace '\s', '').ToUpperInvariant()
  if ($g.Length -ne 64 -or $e.Length -ne 64) { return $false }
  return $g -ceq $e
}
function Step($n, $title, [scriptblock]$body) {
  $script:STEP = $title; $script:CUR = $n; $script:StepFrac = 0
  # A reused tool is already present. It is not shown as a step.
  if (Done $n) { Say (T 'skip' $title) DarkGray; Finish $n; return }
  $script:PhaseKey = $n
  if ($n -ne 'build-app' -and $n -ne 'build-gpu') { Emit-Step $n -1 }
  Prog 0 -Force
  $t = Get-Date; & $body
  Mark $n; Finish $n
  Say (T 'done_in' ((Get-Date) - $t).TotalSeconds) Green
}
function Fetch($key) {
  $d = $DL[$key]; $out = Join-Path $DLDIR $d.file
  if ((Test-Path -LiteralPath $out) -and (Same-Sha256 (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash $d.sha)) { return $out }
  Say (T 'dl' $d.url)
  $nested = [bool]($script:PhaseKey -and $script:PhaseKey -ne $key)
  $req = [Net.HttpWebRequest]::Create($d.url); $req.UserAgent = 'AfroSamuraiRecomp-installer'
  $resp = $req.GetResponse(); $len = $resp.ContentLength; $in = $resp.GetResponseStream()
  $fs = [IO.File]::Create("$out.part"); $buf = New-Object byte[] 1048576; $got = 0
  try {
    while (($r = $in.Read($buf, 0, $buf.Length)) -gt 0) {
      $fs.Write($buf, 0, $r); $got += $r
      if ($len -gt 0) {
        $detail = (T 'dl_prog' $d.file ($got / 1MB) ($len / 1MB))
        if ($nested) {
          # Keep the phase on its own measurement. This file is not that total.
          Prog $script:StepFrac $detail
        } else {
          Prog ($got / $len * $script:DLSHARE) $detail
          # Content-Length is the byte total. No length means the bar stays indeterminate.
          if ($script:PhaseKey) { Emit-Step $script:PhaseKey -1 $got $len 'download' }
        }
      } elseif ($script:PhaseKey -and -not $nested) {
        Emit-Step $script:PhaseKey -1 0 0 'download'
      }
    }
  } finally { $fs.Close(); $in.Close(); $resp.Close() }
  $h = (Get-FileHash -LiteralPath "$out.part" -Algorithm SHA256).Hash
  if (-not (Same-Sha256 $h $d.sha)) { Remove-Item -LiteralPath "$out.part" -Force; throw (T 'sha_bad' $d.file $h $d.sha) }
  Move-Item -LiteralPath "$out.part" $out -Force
  Say (T 'sha_ok' ((Get-Item -LiteralPath $out).Length / 1MB))
  return $out
}
$TAR = Join-Path $env:SystemRoot 'System32\tar.exe'
function Expand-ZipCounted($arc, $dest, [int]$strip, [string]$activity = 'extract') {
  # Zip central directory gives a real entry count. tar.xz is not counted here:
  # listing it decompresses the archive a second time.
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $z = [IO.Compression.ZipFile]::OpenRead($arc)
  try {
    $entries = @()
    foreach ($e in $z.Entries) {
      if (-not $e.FullName -or $e.FullName.EndsWith('/') -or $e.FullName.EndsWith('\')) { continue }
      $rel = $e.FullName.Replace('/', '\')
      if ($strip -gt 0) {
        $parts = $rel.Split('\')
        if ($parts.Length -le $strip) { continue }
        $rel = ($parts | Select-Object -Skip $strip) -join '\'
      }
      $entries += [pscustomobject]@{ Entry = $e; Rel = $rel }
    }
    $total = $entries.Count
    $n = 0
    if ($script:PhaseKey) { Emit-Step $script:PhaseKey -1 0 $total $activity }
    foreach ($item in $entries) {
      $e = $item.Entry
      $rel = $item.Rel
      $target = Join-Path $dest $rel
      $parent = Split-Path $target -Parent
      if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
      [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $target, $true)
      $n++
      if ($script:PhaseKey) { Emit-Step $script:PhaseKey -1 $n $total $activity }
    }
  } finally { $z.Dispose() }
}
function Ensure-ManagedXz {
  # Full path. Never `xz` from PATH: a missing xz is what made tar fail.
  $dir = Join-Path $TOOLS 'xz'
  $exe = Join-Path $dir 'xz.exe'
  $dll = Join-Path $dir 'liblzma.dll'
  if ((Test-Path -LiteralPath $exe) -and (Test-Path -LiteralPath $dll)) { return $exe }
  $zip = Fetch 'xz'
  $stage = Join-Path $TOOLS 'xz-stage'
  if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
  # This count is the small xz zip, not the LLVM archive. Its own activity
  # keeps that 53/53 from being shown as LLVM extraction finished.
  Expand-ZipCounted $zip $stage 0 'unpack'
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  Copy-Item -LiteralPath (Join-Path $stage 'bin_x86-64\xz.exe') -Destination $exe -Force
  Copy-Item -LiteralPath (Join-Path $stage 'bin_x86-64\liblzma.dll') -Destination $dll -Force
  Remove-Item -LiteralPath $stage -Recurse -Force
  if (-not (Test-Path -LiteralPath $exe) -or -not (Test-Path -LiteralPath $dll)) {
    throw 'managed xz.exe was not unpacked from xz-5.8.1-windows.zip'
  }
  return $exe
}
function Expand-TarXz($arc, $dest, [int]$strip) {
  # Decompress with the xz.exe this installer unpacked. Then give tar.exe only
  # the plain .tar. The verified .tar.xz in downloads\ is not deleted.
  $xz = Ensure-ManagedXz
  if (-not (Test-Path -LiteralPath $TAR)) { throw "tar.exe is missing at $TAR" }
  Say "    xz: $xz"
  Say "    tar: $TAR"
  $tarOut = Join-Path $dest 'payload.tar'
  if (Test-Path -LiteralPath $tarOut) { Remove-Item -LiteralPath $tarOut -Force }
  $compressed = [int64](Get-Item -LiteralPath $arc).Length
  # The .tar.xz length is a real total for this phase. It is input bytes, not
  # the unpacked tree. Stop one byte short of the end so the bar does not sit
  # at 100% while xz is still flushing. tar itself has no reliable total.
  if ($script:PhaseKey) {
    if ($compressed -gt 0) { Emit-Step $script:PhaseKey -1 0 $compressed 'decompress' }
    else { Emit-Step $script:PhaseKey -1 0 0 'decompress' }
  }
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $xz
  $psi.Arguments = '-dc'
  $psi.UseShellExecute = $false
  $psi.RedirectStandardInput = $true
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.CreateNoWindow = $true
  $proc = New-Object System.Diagnostics.Process
  $proc.StartInfo = $psi
  # .NET Framework writes the console input encoding's preamble into a redirected
  # stdin when the process starts. On a UTF-8 console (code page 65001, or Windows'
  # "UTF-8 worldwide" option) that is EF BB BF ahead of the archive and xz stops
  # with "File format not recognized". Start it with an encoding without one.
  $oldIn = $null
  try {
    $oldIn = [Console]::InputEncoding
    if ($oldIn.GetPreamble().Length -gt 0) { [Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false) }
  } catch { $oldIn = $null }
  try { [void]$proc.Start() } finally {
    if ($oldIn) { try { [Console]::InputEncoding = $oldIn } catch {} }
  }
  $fs = [IO.File]::Create($tarOut)
  $err = ''
  try {
    $copy = $proc.StandardOutput.BaseStream.CopyToAsync($fs)
    $errTask = $proc.StandardError.ReadToEndAsync()
    $src = [IO.File]::Open($arc, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $stdin = $proc.StandardInput.BaseStream
    try {
      $buf = New-Object byte[] 1048576
      $sent = [int64]0
      while (($r = $src.Read($buf, 0, $buf.Length)) -gt 0) {
        $stdin.Write($buf, 0, $r)
        $sent += $r
        if ($script:PhaseKey -and $compressed -gt 1) {
          $shown = $sent
          if ($shown -ge $compressed) { $shown = $compressed - 1 }
          Emit-Step $script:PhaseKey -1 $shown $compressed 'decompress'
        }
      }
    } finally { $stdin.Close(); $src.Close() }
    $err = $errTask.GetAwaiter().GetResult()
    $copy.Wait()
  } finally { $fs.Close() }
  $proc.WaitForExit()
  $tarLen = if (Test-Path -LiteralPath $tarOut) { (Get-Item -LiteralPath $tarOut).Length } else { 0 }
  if ($proc.ExitCode -ne 0 -or $tarLen -lt 1) {
    if (Test-Path -LiteralPath $tarOut) { Remove-Item -LiteralPath $tarOut -Force }
    throw "managed xz failed ($($proc.ExitCode)) $err"
  }
  # tar.exe does not report a file total unless the archive is read twice.
  # Drop the decompress fraction before it starts so the bar cannot stay full.
  if ($script:PhaseKey) { Emit-Step $script:PhaseKey -1 0 0 'extract' }
  $arg = @('-xf', $tarOut, '-C', $dest)
  if ($strip -gt 0) { $arg += @('--strip-components', "$strip") }
  $prev = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  & $TAR @arg 2>&1 | ForEach-Object { Write-InstallLog "$_" -Bare }
  $code = $LASTEXITCODE
  $ErrorActionPreference = $prev
  Remove-Item -LiteralPath $tarOut -Force
  if ($code -ne 0) { throw "tar.exe exit $code after xz. The downloaded archive was kept." }
  $left = @(Get-ChildItem -LiteralPath $dest -Force | Where-Object { $_.Name -ne 'payload.tar' })
  if ($left.Count -lt 1) { throw 'extract produced no files. The downloaded archive was kept.' }
}
function Extract($arc, $dest, [int]$strip = 0, [switch]$Into) {
  $isZip = $arc -match '\.(zip|whl)$'
  $isTarXz = $arc -match '\.tar\.xz$'
  if ($script:PhaseKey) { Emit-Step $script:PhaseKey -1 0 0 'extract' }
  # Into a temporary folder that is renamed at the end, so a failed extract
  # never replaces a finished folder. The file in downloads\ stays.
  Prog $script:DLSHARE (T 'extracting' (Split-Path $arc -Leaf)) -Force
  $tmp = if ($Into) { $dest } else { "$dest.partial" }
  if (-not $Into -and (Test-Path -LiteralPath $tmp)) { Remove-Item -LiteralPath $tmp -Recurse -Force }
  New-Item -ItemType Directory -Force -Path $tmp | Out-Null
  $ok = $false
  $fail = $null
  if ($isTarXz) {
    try { Expand-TarXz $arc $tmp $strip; $ok = $true } catch { $fail = $_.Exception.Message; Say $fail Yellow }
  } elseif ($isZip) {
    try { Expand-ZipCounted $arc $tmp $strip; $ok = $true } catch { $fail = $_.Exception.Message; Say (T 'tar_fail' $fail) Yellow }
  }
  if (-not $ok) {
    if (-not $Into -and (Test-Path -LiteralPath $tmp)) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    # Keep the checksum or the tool's exit code. Do not replace it with "tar.exe missing".
    if ($fail) { throw $fail }
    throw (T 'cant_extract' $arc)
  }
  if (-not $Into) {
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Rename-Item -LiteralPath $tmp (Split-Path $dest -Leaf)
  }
  Prog 1 -Force
}
function Run($exe, [string[]]$a, $wd = $null) {
  # Runs a program, streaming its output to the console and the log; throws on failure.
  if ($wd) { Push-Location -LiteralPath $wd }
  $lines = New-Object System.Collections.Generic.List[string]
  try {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    & $exe @a 2>&1 | ForEach-Object {
      $s = "$_"; $lines.Add($s); Write-InstallLog $s -Bare
      if ($s -match '^AFROPHASE codegen') {
        $script:CodegenLog = $true
        $script:PhaseT0 = Get-Date
        Emit-Step 'codegen' -1 0 0 'generate'
      } elseif ($s -match '^AFROPHASE compile') {
        $script:CodegenLog = $false
        if ($script:PhaseT0) { Say ("    codegen took {0:n0} s" -f ((Get-Date) - $script:PhaseT0).TotalSeconds) }
        $script:PhaseT0 = $null
        Emit-Step 'compile' -1 0 0 'compile'
      } elseif ($s -match 'Receiving objects:\s+\d+%\s+\((\d+)/(\d+)\)') {
        if ($script:PhaseKey) { Emit-Step $script:PhaseKey -1 ([long]$Matches[1]) ([long]$Matches[2]) 'fetch' }
      } elseif ($script:PhaseKey -ne 'codegen' -and $s -match '^\s*\[(\d+)/(\d+)\]') {
        $d = [int]$Matches[1]; $t = [int]$Matches[2]
        $script:NinjaSteps = "$d/$t"
        $key = if ($script:PhaseKey -eq 'gpu') { 'gpu' } else { 'compile' }
        $act = if ($key -eq 'gpu') { 'graphics' } else { 'compile' }
        Emit-Step $key -1 $d $t $act
        Prog ($script:BASE + (1.0 - $script:BASE) * [double]$d / [double]$t) (T 'compiling' $d $t)
      } elseif ($s -notmatch '^AFROPHASE ' -and $s -notmatch '^\[') { Say "    $s" }
      if ($script:CodegenLog) {
        try { Add-LogLine (Join-Path $Root 'codegen_win.log') $s | Out-Null } catch { }
      }
    }
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    if ($code -ne 0) { throw (T 'exit_code' $exe $code) }
    return $lines
  } finally { if ($wd) { Pop-Location } }
}
$script:GitOwned = @()
function Add-OwnedGit([string]$path) {
  if (-not $path) { return }
  $script:GitOwned += $path
}
function Convert-GitOwnedPath([string]$path) {
  $full = [IO.Path]::GetFullPath($path).TrimEnd('\', '/')
  return ($full -replace '\\', '/')
}
function Test-BroadSafeDirectory([string]$value) {
  if (-not $value -or $value -eq '*' -or $value -eq '/*') { return $true }
  $p = ($value -replace '\\', '/').TrimEnd('/')
  if ($p -eq '*' -or $p -eq '/*') { return $true }
  if ($p -match '^[A-Za-z]:$') { return $true }
  if ($p -match '^//[^/]+/[^/]+$') { return $true }
  return $false
}
function Get-OwnedGitDirectories([string[]]$extra) {
  $seen = @{}
  $out = New-Object System.Collections.Generic.List[string]
  foreach ($raw in @($script:GitOwned + $extra)) {
    if (-not $raw) { continue }
    $path = Convert-GitOwnedPath $raw
    if ($path.Contains('*') -or (Test-BroadSafeDirectory $path)) {
      throw "Refusing to mark a broad Git directory safe: $path"
    }
    foreach ($value in @($path, "$path/*")) {
      if (Test-BroadSafeDirectory $value) { throw "Refusing Git safe.directory $value" }
      if (-not $seen.ContainsKey($value)) { $seen[$value] = $true; $out.Add($value) }
    }
  }
  return $out
}
function Git([string[]]$a, $wd = $null) {
  # Command-scoped safe.directory only. The user's Git configuration files are not modified.
  $extra = @()
  if ($wd) { $extra += $wd }
  for ($i = 0; $i -lt $a.Count; $i++) {
    if ($a[$i] -eq '-C' -and ($i + 1) -lt $a.Count) { $extra += $a[$i + 1] }
  }
  $values = @(Get-OwnedGitDirectories $extra)
  $saved = @{}
  $start = 0
  if ($env:GIT_CONFIG_COUNT -match '^\d+$') { $start = [int]$env:GIT_CONFIG_COUNT }
  $saved['GIT_CONFIG_COUNT'] = $env:GIT_CONFIG_COUNT
  $n = $start
  $prefix = @()
  foreach ($value in $values) {
    $keyName = "GIT_CONFIG_KEY_$n"
    $valName = "GIT_CONFIG_VALUE_$n"
    $saved[$keyName] = [Environment]::GetEnvironmentVariable($keyName)
    $saved[$valName] = [Environment]::GetEnvironmentVariable($valName)
    Set-Item -Path "Env:$keyName" -Value 'safe.directory'
    Set-Item -Path "Env:$valName" -Value $value
    $prefix += @('-c', "safe.directory=$value")
    $n++
  }
  $env:GIT_CONFIG_COUNT = "$n"
  try { Run (Join-Path $TOOLS 'git\cmd\git.exe') ($prefix + $a) $wd }
  finally {
    foreach ($name in @($saved.Keys)) {
      if ($null -eq $saved[$name]) { Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue }
      else { Set-Item -Path "Env:$name" -Value $saved[$name] }
    }
  }
}
function Test-ExactInstallerDir([string]$path, [string]$leaf) {
  $full = [IO.Path]::GetFullPath($path).TrimEnd('\')
  $root = [IO.Path]::GetFullPath($Root).TrimEnd('\')
  $expected = [IO.Path]::GetFullPath((Join-Path $root $leaf)).TrimEnd('\')
  if (-not [string]::Equals($full, $expected, [StringComparison]::OrdinalIgnoreCase)) { return $false }
  if (-not $full.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { return $false }
  $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
  if ($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return $false }
  return $true
}
function Remove-IncompleteClone([string]$path, [string]$leaf) {
  # A directory with .git is resumed, never deleted. Anything that is not this
  # installer's own rexglue-sdk or afrorecomp folder is left untouched.
  if (-not (Test-Path -LiteralPath $path)) { return }
  if (Test-Path -LiteralPath (Join-Path $path '.git')) { return }
  if ($leaf -ne 'rexglue-sdk' -and $leaf -ne 'afrorecomp') {
    throw "Refusing to delete $path. This installer only replaces its own incomplete clone."
  }
  if (-not (Test-ExactInstallerDir $path $leaf)) {
    throw "Refusing to delete $path. It is not the installer's $leaf folder."
  }
  Remove-Item -LiteralPath $path -Recurse -Force
}

function Get-TreeFiles([string]$base, [string[]]$skip) {
  # Relative paths of the files under $base. Anything named .git (a clone's
  # folder or a worktree's file), a relative path in $skip and a reparse
  # point are left out.
  $out = New-Object System.Collections.Generic.List[string]
  if (-not [IO.Directory]::Exists($base)) { return ,$out }
  $stack = New-Object System.Collections.Generic.Stack[string]
  $stack.Push($base)
  while ($stack.Count -gt 0) {
    $d = $stack.Pop()
    foreach ($sub in [IO.Directory]::GetDirectories($d)) {
      $rel = $sub.Substring($base.Length).TrimStart('\')
      if ([IO.Path]::GetFileName($sub) -eq '.git' -or $skip -contains $rel) { continue }
      if ([IO.File]::GetAttributes($sub) -band [IO.FileAttributes]::ReparsePoint) { continue }
      $stack.Push($sub)
    }
    foreach ($f in [IO.Directory]::GetFiles($d)) {
      $rel = $f.Substring($base.Length).TrimStart('\')
      if ([IO.Path]::GetFileName($f) -ne '.git' -and $skip -notcontains $rel) { $out.Add($rel) }
    }
  }
  return ,$out
}
function Test-SameBytes([string]$a, [string]$b) {
  $ib = New-Object IO.FileInfo $b
  if (-not $ib.Exists -or (New-Object IO.FileInfo $a).Length -ne $ib.Length) { return $false }
  return [Linq.Enumerable]::SequenceEqual([byte[]][IO.File]::ReadAllBytes($a), [byte[]][IO.File]::ReadAllBytes($b))
}
function Sync-SourceTree([string]$src, [string]$dest, [string[]]$keep) {
  # Makes $dest hold the same source files as $src (minus .git and disc
  # images). Equal files are not written, so their timestamps stay older than
  # the build outputs. A written file gets the current time: a snapshot with
  # old timestamps must still look newer than the objects built before it.
  # Paths in $keep (build folder, codegen output, game-derived files) are
  # neither written nor removed.
  $files = @((Get-TreeFiles $src @()) | Where-Object { [IO.Path]::GetExtension($_) -notin '.iso', '.xex', '.pe' })
  $r = [pscustomobject]@{ Changed = 0; Same = 0; Removed = 0; ChangedFiles = @(); RemovedFiles = @() }
  $wanted = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
  New-Item -ItemType Directory -Force -Path $dest | Out-Null
  $n = 0
  # Each file compared or copied advances this count. There is no time estimate.
  Emit-Step 'repo' -1 0 $files.Count 'copy'
  foreach ($rel in $files) {
    [void]$wanted.Add($rel)
    $n++
    $isKept = $false
    foreach ($k in $keep) { if ($rel -eq $k -or $rel.StartsWith($k + '\', [StringComparison]::OrdinalIgnoreCase)) { $isKept = $true; break } }
    if ($isKept) { continue }
    $from = Join-Path $src $rel
    $target = Join-Path $dest $rel
    if (Test-SameBytes $from $target) {
      $r.Same++
    } else {
      $parent = Split-Path $target -Parent
      if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
      Copy-Item -LiteralPath $from -Destination $target -Force
      [IO.File]::SetLastWriteTimeUtc($target, [DateTime]::UtcNow)
      $r.Changed++
      $r.ChangedFiles += $rel
    }
    Emit-Step 'repo' -1 $n $files.Count 'copy'
  }
  # Files the new source no longer has. Kept paths and a nested .git are not
  # looked at; a .git at the top of an old clone is removed with the rest.
  $topGit = Join-Path $dest '.git'
  if ((Test-Path -LiteralPath $topGit) -and -not ((Get-Item -LiteralPath $topGit -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    Remove-Item -LiteralPath $topGit -Recurse -Force
    $r.Removed++
    $r.RemovedFiles += '.git'
  }
  foreach ($rel in (Get-TreeFiles $dest $keep)) {
    if ($wanted.Contains($rel)) { continue }
    $ext = [IO.Path]::GetExtension($rel)
    if ($ext -in '.iso', '.xex', '.pe') { continue }
    Remove-Item -LiteralPath (Join-Path $dest $rel) -Force
    $r.Removed++
    $r.RemovedFiles += $rel
  }
  return $r
}
function Get-BuildToolKey {
  # What the compiled objects in app\out depend on beyond their sources.
  # CMake does not notice a different compiler at the same path.
  $parts = @()
  foreach ($rel in 'tools\llvm\bin\clang++.exe', 'tools\cmake\bin\cmake.exe', 'tools\ninja\ninja.exe') {
    $f = Get-Item -LiteralPath (Join-Path $Root $rel) -ErrorAction SilentlyContinue
    $parts += if ($f) { "$rel=$($f.Length)" } else { "$rel=none" }
  }
  foreach ($rel in 'tools\msvc\VC\Tools\MSVC', 'tools\msvc\Windows Kits\10\Lib') {
    $d = Join-Path $Root $rel
    $names = if (Test-Path -LiteralPath $d) { (@(Get-ChildItem -LiteralPath $d -Directory | ForEach-Object { $_.Name }) | Sort-Object) -join ',' } else { 'none' }
    $parts += "$rel=$names"
  }
  return ($parts -join ';')
}

# ---------------------------------------------------------------- setup ------
# Language first (before anything else). Default: Windows UI language if supported.
$ui = try { (Get-UICulture).TwoLetterISOLanguageName } catch { 'en' }
$script:LI = [math]::Max(0, $LANGS.IndexOf($ui))
if ($NonInteractive -and -not $Language) { throw 'NonInteractive requires -Language en, es, fr, or it.' }
if ($Language) { $script:LI = $LANGS.IndexOf($Language) }
else {
  Write-Host ''
  Write-Host 'AfroSamuraiRecomp - Language / Idioma / Langue / Lingua' -ForegroundColor Yellow
  for ($i = 0; $i -lt 4; $i++) { Write-Host ("  [{0}] {1}" -f $i, $LANG_NAMES[$i]) }
  $a = Read-Host ("[{0}]" -f $script:LI)
  if ($a -match '^\s*([0-3])\s*$') { $script:LI = [int]$Matches[1] }
  elseif ($a -and $LANGS.IndexOf($a.Trim().ToLower()) -ge 0) { $script:LI = $LANGS.IndexOf($a.Trim().ToLower()) }
}
$Language = $LANGS[$script:LI]
Write-Host ''
Write-Host (T 'title') -ForegroundColor Yellow
Write-Host (T 'intro')
Write-Host ''
$here = $PSScriptRoot
# A git clone of this repo (a ZIP download has no .git: it fetches the source from GitHub instead).
$inRepo = (Test-Path -LiteralPath (Join-Path $here 'build_win.bat')) -and (Test-Path -LiteralPath (Join-Path (Split-Path $here -Parent) '.git'))
function PickFolder($start) {
  try {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.Application]::EnableVisualStyles()
    $f = New-Object System.Windows.Forms.FolderBrowserDialog
    $f.Description = T 'pick_desc'
    $f.ShowNewFolderButton = $true
    if (Test-Path -LiteralPath $start) { $f.SelectedPath = $start }
    $top = New-Object System.Windows.Forms.Form -Property @{ TopMost = $true }
    if ($f.ShowDialog($top) -eq 'OK') { return $f.SelectedPath }
    return $null
  } catch { return (Ask (T 'ask_folder') $start) }
}
function CheckPath($p) {
  $w = @()
  if ($p -match '[^\x20-\x7E]') { $w += T 'w_ascii' }
  if ($p.Length -gt 80) { $w += T 'w_long' $p.Length }
  try { $fr = (New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($p))).AvailableFreeSpace / 1GB; if ($fr -lt $NEED_GB) { $w += T 'w_space' $fr $NEED_GB } } catch { }
  $w | ForEach-Object { Write-Host "$(T 'warning'): $_" -ForegroundColor Yellow }
}
$NAME = if ($inRepo) { 'AfroSamuraiRecomp-install' } else { 'AfroSamuraiRecomp' }
function Test-VolumeOrShareRoot([string]$Path) {
  if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
  $p = $Path.Trim() -replace '/', '\'
  if ($p -match '^[A-Za-z]:\\?$') { return $true }
  $trim = $p.TrimEnd('\')
  if ($trim -match '^\\\\[^\\]+\\[^\\]+$') { return $true }
  return $false
}
# A new install is created inside the chosen parent. D:\ becomes
# D:\AfroSamuraiRecomp. A parent that already ends with that name is not nested again.
# An update keeps the folder that was selected: that folder is the install itself.
function Resolve-NewInstallRoot([string]$Path) {
  if ([string]::IsNullOrWhiteSpace($Path)) { throw 'Install folder is empty.' }
  $p = $Path.Trim().Trim('"') -replace '/', '\'
  try { $p = [IO.Path]::GetFullPath($p) } catch { throw "Install folder is not a valid path: $Path" }
  $leaf = [IO.Path]::GetFileName($p.TrimEnd('\'))
  if ($leaf -eq 'AfroSamuraiRecomp' -and -not (Test-VolumeOrShareRoot $p)) {
    return $p.TrimEnd('\')
  }
  if (Test-VolumeOrShareRoot $p) {
    $root = [IO.Path]::GetPathRoot($p)
    if (-not $root.EndsWith('\')) { $root += '\' }
    return (Join-Path $root 'AfroSamuraiRecomp')
  }
  return (Join-Path $p.TrimEnd('\') 'AfroSamuraiRecomp')
}
if ($NonInteractive -and -not $InstallDir) { throw 'NonInteractive requires -InstallDir.' }
$Root = $InstallDir
while (-not $Root) {
  $start = if ($inRepo) { Split-Path (Split-Path $here -Parent) -Parent } else { $here }
  Write-Host (T 'choose_folder')
  $parent = PickFolder $start
  if (-not $parent) { $parent = Ask (T 'no_folder') $start }
  $cand = if ($inRepo) { [IO.Path]::GetFullPath((Join-Path $parent $NAME)) } else { Resolve-NewInstallRoot $parent }
  if ($inRepo -and $cand -eq (Split-Path $here -Parent)) { $cand = "$cand-install" }
  Write-Host ''; Write-Host (T 'install_folder' $cand) -ForegroundColor Cyan
  CheckPath $cand
  if (YesNo (T 'install_there') 'y') { $Root = $cand }
}
if ($Update) {
  try { $Root = [IO.Path]::GetFullPath($Root.Trim().Trim('"')) } catch { throw "Install folder is not a valid path: $Root" }
  if (Test-VolumeOrShareRoot $Root) {
    throw 'Update needs the existing AfroSamuraiRecomp folder, not the drive or share itself. Nothing was deleted.'
  }
} elseif (-not $inRepo) {
  $Root = Resolve-NewInstallRoot $Root
} else {
  try { $Root = [IO.Path]::GetFullPath($Root.Trim().Trim('"')) } catch { throw "Install folder is not a valid path: $Root" }
}
if (Test-VolumeOrShareRoot $Root) {
  throw 'Refusing to install directly on a drive or share root. Nothing was deleted.'
}
[IO.Directory]::CreateDirectory($Root) | Out-Null
$LOG = Join-Path $Root 'install.log'; $script:LOG = $LOG
$TOOLS = Join-Path $Root 'tools'; $DLDIR = Join-Path $Root 'downloads'; $STAMPS = Join-Path $Root '.install'
$REPO = Join-Path $Root 'afrorecomp'
$here2 = Split-Path $here -Parent
# Started from a clone of this repo: build that checkout (no GitHub access needed for the source).
if ($inRepo -and -not $PSBoundParameters.ContainsKey('RepoUrl')) { $RepoUrl = $here2 }
if ($inRepo -and ((Split-Path $here2 -Parent) -eq $Root)) { $REPO = $here2 }   # run from an existing clone
New-Item -ItemType Directory -Force -Path $TOOLS, $DLDIR, $STAMPS | Out-Null
$t0 = Get-Date
Say (T 'install_folder' $Root)
Say (T 'log_file' $LOG)

function Test-AfroRunning {
  $procs = @(Get-Process -Name 'afrorecomp' -ErrorAction SilentlyContinue)
  if ($procs.Count -gt 0) {
    throw 'afrorecomp.exe is running. Close the game. This installer will not stop it.'
  }
}
# Files that mean a previous download is still usable. Build stamps are not here:
# an update always clears those so the same version number is compiled again.
$UPDATE_REUSE = @{
  'git' = 'tools\git\cmd\git.exe'
  'cmake' = 'tools\cmake\bin\cmake.exe'
  'ninja' = 'tools\ninja\ninja.exe'
  'python' = 'tools\python\python.exe'
  'llvm' = 'tools\llvm\bin\clang++.exe'
  'msvc' = 'tools\msvc\setup_x64.bat'
  'sdk' = 'sdk\win-amd64\bin\rexglue.exe'
  'rexglue-src' = 'rexglue-sdk\.git'
  'gamedata' = 'gamedata\default.xex'
}
$UPDATE_REBUILD = @('build-app', 'build-gpu', 'build')
function Repair-UpdateStamps {
  foreach ($name in @($UPDATE_REUSE.Keys)) {
    $stamp = Join-Path $STAMPS "$name.done"
    $target = Join-Path $Root $UPDATE_REUSE[$name]
    $exists = Test-Path -LiteralPath $target
    $marked = Test-Path -LiteralPath $stamp
    if ($exists -and -not $marked) {
      New-Item -ItemType File -Force -Path $stamp | Out-Null
      Say "    reuse existing $name"
    } elseif ($marked -and -not $exists) {
      Remove-Item -LiteralPath $stamp -Force
      Say "    ${name} is missing and will be downloaded again"
    }
  }
  if ((Test-Path -LiteralPath (Join-Path $Root 'gamedata\default.xex')) -and -not (Test-Path -LiteralPath (Join-Path $STAMPS 'iso-hash.done'))) {
    New-Item -ItemType File -Force -Path (Join-Path $STAMPS 'iso-hash.done') | Out-Null
    Say '    extracted game already present; ISO will not be requested'
  }
  foreach ($name in $UPDATE_REBUILD) {
    Remove-Item -LiteralPath (Join-Path $STAMPS "$name.done") -Force -ErrorAction SilentlyContinue
  }
  Remove-Item -LiteralPath (Join-Path $STAMPS 'build.rev') -Force -ErrorAction SilentlyContinue
  Say '    same-version refresh: compile stamps cleared; embedded source will be built'
}
try {
  if ($Update) {
    Test-AfroRunning
    $play = Test-Path -LiteralPath (Join-Path $Root 'game\Play.bat')
    $bin = Test-Path -LiteralPath (Join-Path $Root 'game\afrorecomp.exe')
    if (-not $play -and -not $bin) { throw 'This folder is not an AfroSamuraiRecomp installation (game\Play.bat is missing).' }
    $installedVer = ''
    $verFile = Join-Path $Root 'afrorecomp\app\VERSION'
    if (Test-Path -LiteralPath $verFile) { $installedVer = (Get-Content -LiteralPath $verFile -Raw).Trim() }
    elseif (Test-Path -LiteralPath (Join-Path $STAMPS 'build.rev')) {
      $rev = (Get-Content -LiteralPath (Join-Path $STAMPS 'build.rev') -Raw).Trim()
      if ($rev.StartsWith('local-')) {
        $rest = $rev.Substring(6)
        $dash = $rest.IndexOf('-')
        $installedVer = if ($dash -ge 0) { $rest.Substring(0, $dash) } else { $rest }
      }
    }
    if (-not $installedVer) { $installedVer = 'not recorded' }
    Repair-UpdateStamps
    Say "    update $installedVer -> $LOCAL_TREE_VERSION"
    if ($installedVer -eq $LOCAL_TREE_VERSION) {
      Say '    same version: program files are rebuilt and replaced only after the build succeeds'
    }
  }
  # ------------------------------------------------------- questions -------
  # Every question is asked here, before any download, so the rest runs unattended.
  Write-Host ''; Write-Host (T 'all_q') -ForegroundColor Cyan
  $script:STEP = 'choose game data'
  $haveGame = Test-Path -LiteralPath (Join-Path $Root 'gamedata\default.xex')
  if ($NonInteractive -and -not $haveGame -and -not $GameDataDir -and -not $Iso) {
    throw 'NonInteractive requires -Iso or -GameDataDir.'
  }
  if (-not $haveGame -and -not $GameDataDir -and -not $Iso) {
    Say (T 'sel_iso')
    try {
      Add-Type -AssemblyName System.Windows.Forms
      $dlg = New-Object System.Windows.Forms.OpenFileDialog
      $dlg.Title = 'Select your Afro Samurai ISO (Europe or USA)'; $dlg.Filter = 'Disc images (*.iso)|*.iso|All files (*.*)|*.*'
      if ($dlg.ShowDialog() -eq 'OK') { $Iso = $dlg.FileName }
    } catch { }
    if (-not $Iso) { $Iso = Ask (T 'iso_path') '' }
  }
  if ($Iso -and -not (Test-Path -LiteralPath $Iso)) { throw (T 'iso_nf' $Iso) }
  if ($GameDataDir -and -not (Test-Path -LiteralPath (Join-Path $GameDataDir 'default.xex'))) { throw (T 'no_xex' $GameDataDir) }

  # ------------------------------------------------------- checks ----------
  $script:STEP = 'system checks'
  if (-not [Environment]::Is64BitOperatingSystem) { throw (T 'need64') }
  $free = (Get-PSDrive -Name ($Root.Substring(0, 1))).Free / 1GB
  Say (T 'free' $Root.Substring(0, 2) $free)
  if (-not (Done 'build') -and $free -lt $NEED_GB) { throw (T 'nospace' $NEED_GB $free) }
  $gpus = @(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -notmatch 'Basic|Remote|Virtual|Parsec|Meta' })
  if ($gpus.Count -eq 0) { Say (T 'nogpu') Yellow }
  else { $gpus | ForEach-Object { Say (T 'gpu' $_.Name $_.DriverVersion) } ; Say (T 'gpu_need') }

  $script:STEP = 'verify ISO'; $script:CUR = 'iso-hash'
  if ($Iso -and -not (Done 'iso-hash')) {
    Prog 0.1 (T 'iso_sha_prog') -Force
    # A missing or unreadable file throws here. A different SHA-1 does not.
    $isoFull = [IO.Path]::GetFullPath($Iso)
    $isoLen = [int64](Get-Item -LiteralPath $isoFull).Length
    $notePath = Join-Path $STAMPS 'iso-unverified.note'
    function Read-IsoNote {
      if (-not (Test-Path -LiteralPath $notePath)) { return $null }
      $m = @{}
      foreach ($line in [IO.File]::ReadAllLines($notePath)) {
        $eq = $line.IndexOf('=')
        if ($eq -gt 0) { $m[$line.Substring(0, $eq)] = $line.Substring($eq + 1) }
      }
      return $m
    }
    function Test-UnverifiedNote {
      # Same path and length: the SHA-1 already recorded still describes this file.
      # The length does not accept the disc. It only avoids hashing those bytes again.
      $m = Read-IsoNote
      if (-not $m) { return $false }
      $recorded = ''
      try { $recorded = [IO.Path]::GetFullPath([string]$m['path']).TrimEnd('\') } catch { return $false }
      if (-not [string]::Equals($recorded, $isoFull.TrimEnd('\'), [StringComparison]::OrdinalIgnoreCase)) { return $false }
      $recordedLen = [int64]0
      if (-not [int64]::TryParse([string]$m['length'], [ref]$recordedLen) -or $recordedLen -ne $isoLen -or $isoLen -le 0) { return $false }
      $act = ([string]$m['actual'] -replace '\s', '').ToUpperInvariant()
      $exp = ([string]$m['expected'] -replace '\s', '').ToUpperInvariant()
      if ($act.Length -ne 40 -or $exp -ne $ISO_SHA1 -or $act -eq $ISO_SHA1) { return $false }
      $script:IsoActual = $act
      return $true
    }
    function Write-IsoNote([string]$actual) {
      $tmp = "$notePath.tmp"
      $body = "path=$isoFull`r`nlength=$isoLen`r`nactual=$actual`r`nexpected=$ISO_SHA1`r`n"
      [IO.File]::WriteAllText($tmp, $body, (New-Object Text.UTF8Encoding $false))
      Move-Item -LiteralPath $tmp -Destination $notePath -Force
    }
    $script:IsoActual = ''
    if (Test-UnverifiedNote) {
      Say (T 'iso_continue' $script:IsoActual $ISO_SHA1) Yellow
    } else {
      if (Test-Path -LiteralPath $notePath) { Remove-Item -LiteralPath $notePath -Force }
      Say (T 'iso_check')
      $h = (Get-FileHash -LiteralPath $isoFull -Algorithm SHA1).Hash
      if ($h -eq $ISO_SHA1) {
        Say (T 'iso_ok') Green
        Mark 'iso-hash'
        if (Test-Path -LiteralPath $notePath) { Remove-Item -LiteralPath $notePath -Force }
      } else {
        Write-IsoNote $h
        Say (T 'iso_continue' $h $ISO_SHA1) Yellow
      }
    }
  }
  Finish 'iso-hash'

  # MSVC is pinned ($MSVC_VERSION): from 14.52 its C++ library refuses Clang 20
  # (STL1000, "expected Clang 22 or newer"). An install that got another version
  # (setups before 0.2.1 took the newest) gets the pinned one again, and the
  # builds made with the other headers are redone.
  $msvcDir = Join-Path $TOOLS 'msvc\VC\Tools\MSVC'
  if ((Done 'msvc') -and (Test-Path -LiteralPath $msvcDir) -and
      -not (Get-ChildItem -LiteralPath $msvcDir -Directory | Where-Object { $_.Name -like "$MSVC_VERSION.*" })) {
    $had = (@(Get-ChildItem -LiteralPath $msvcDir -Directory | ForEach-Object { $_.Name }) -join ',')
    Say "MSVC $had is not the pinned $MSVC_VERSION; installing $MSVC_VERSION again"
    Remove-Item -LiteralPath (Join-Path $TOOLS 'msvc') -Recurse -Force
    Remove-Item -LiteralPath (Join-Path $STAMPS 'msvc.done') -Force
    foreach ($n in 'build-app', 'build-gpu') { Remove-Item -LiteralPath (Join-Path $STAMPS "$n.done") -Force -ErrorAction SilentlyContinue }
    foreach ($rel in 'afrorecomp\app\out', 'rexglue-sdk\out') {
      Remove-Item -LiteralPath (Join-Path $Root $rel) -Recurse -Force -ErrorAction SilentlyContinue
    }
  }

  # Microsoft license for the portable MSVC compiler + Windows SDK (only if not installed yet).
  $script:STEP = 'Microsoft license'
  $needMsvc = -not (Done 'msvc')
  if ($needMsvc -and $NonInteractive -and -not $AcceptMsvcLicense) { throw (T 'lic_no') }
  if ($needMsvc -and -not $AcceptMsvcLicense) {
    $lic = 'https://go.microsoft.com/fwlink/?LinkId=2327714'
    try {
      $m = Invoke-RestMethod -UseBasicParsing -Uri 'https://aka.ms/vs/stable/channel'
      $bt = $m.channelItems | Where-Object { $_.id -eq 'Microsoft.VisualStudio.Product.BuildTools' } | Select-Object -First 1
      $u = ($bt.localizedResources | Where-Object { $_.language -eq 'en-us' } | Select-Object -First 1).license
      if ($u) { $lic = $u }
    } catch { }
    Write-Host ''
    Write-Host (T 'lic1')
    Write-Host (T 'lic2')
    Write-Host "  $lic" -ForegroundColor Cyan
    Say "MSVC license shown: $lic"
    if (-not (YesNo (T 'lic_q') 'n')) { throw (T 'lic_no') }
    $AcceptMsvcLicense = $true; Say 'MSVC license accepted by the user'
  }

  # Shortcuts (answer remembered for re-runs).
  $script:STEP = 'shortcuts question'
  $scFile = Join-Path $STAMPS 'shortcut.answer'
  if (-not $Shortcut -and (Test-Path -LiteralPath $scFile)) { $Shortcut = (Get-Content -LiteralPath $scFile -Raw).Trim() }
  if (-not $Shortcut) {
    if ($NonInteractive) { throw 'NonInteractive requires -Shortcut yes or -Shortcut no.' }
    $Shortcut = if (YesNo (T 'sc_q') 'y') { 'yes' } else { 'no' }
  }
  Set-Content -LiteralPath $scFile -Value $Shortcut

  Write-Host ''
  Write-Host (T 'summary') -ForegroundColor Cyan
  Say (T 's_lang' $LANG_NAMES[$script:LI])
  Say (T 's_folder' $Root)
  Say (T 's_data' $(if ($GameDataDir) { T 's_copied' $GameDataDir } elseif ($Iso) { $Iso } else { T 's_extracted' }))
  Say (T 's_lic' $(if ($needMsvc) { T 's_acc' } else { T 's_msvc' }))
  Say (T 's_sc' $Shortcut)
  Say (T 's_go')
  Write-Host ''

  # ------------------------------------------------------- toolchain -------
  Set-PhasePlan
  Step 'git' (T 't_git') { Extract (Fetch 'mingit') (Join-Path $TOOLS 'git') }
  Step 'cmake' (T 't_cmake') { Extract (Fetch 'cmake') (Join-Path $TOOLS 'cmake') 1 }
  Step 'ninja' 'Ninja' { Extract (Fetch 'ninja') (Join-Path $TOOLS 'ninja') }
  Step 'python' (T 't_python') {
    $p = Join-Path $TOOLS 'python'; Extract (Fetch 'python') $p
    Extract (Fetch 'pycryptodome') $p -Into
    Run (Join-Path $p 'python.exe') @('-c', 'import Crypto.Cipher.AES')
  }
  Step 'llvm' (T 't_llvm') {
    $d = Join-Path $TOOLS 'llvm'; Extract (Fetch 'llvm') $d 1
    if (-not (Test-Path -LiteralPath (Join-Path $d 'bin\clang++.exe'))) { throw (T 'missing1' 'clang++.exe') }
  }
  Step 'msvc' (T 't_msvc') {
    Copy-Item -LiteralPath (Fetch 'pmsvc') (Join-Path $TOOLS 'portable-msvc.py') -Force
    $py = Join-Path $TOOLS 'python\python.exe'
    Say ('    ' + (T 'lic1')); Say ('    ' + (T 'lic2'))
    if (-not $AcceptMsvcLicense) { throw (T 'lic_not') }   # asked at the start
    Run $py @('portable-msvc.py', '--accept-license', '--msvc-version', $MSVC_VERSION, '--sdk-version', $WINSDK_VERSION) $TOOLS
    if (-not (Test-Path -LiteralPath (Join-Path $TOOLS 'msvc\setup_x64.bat'))) { throw (T 'missing1' 'msvc\setup_x64.bat') }
    Remove-Item -LiteralPath (Join-Path $TOOLS 'downloads') -Recurse -Force -ErrorAction SilentlyContinue
  }

  # ------------------------------------------------------- sources ---------
  # The source step always runs (no stamp), so a re-run picks up new commits.
  # Only these two directories are marked safe for this process. Not the drive.
  Add-OwnedGit (Join-Path $Root 'rexglue-sdk')
  Add-OwnedGit $REPO
  Remove-Item -LiteralPath (Join-Path $STAMPS 'repo.done') -Force -ErrorAction SilentlyContinue
  $REV = $null
  if ($LocalTree) {
    $LocalTree = [IO.Path]::GetFullPath($LocalTree.Trim().Trim('"'))
    $verFile = Join-Path $LocalTree 'app\VERSION'
    if (-not (Test-Path -LiteralPath (Join-Path $LocalTree 'tools\build_win.bat'))) {
      throw "Local source is missing tools\build_win.bat: $LocalTree"
    }
    if (-not (Test-Path -LiteralPath $verFile)) { throw "Local source is missing app\VERSION: $LocalTree" }
    $localVer = (Get-Content -LiteralPath $verFile -Raw).Trim()
    if ($NonInteractive -and $localVer -ne $LOCAL_TREE_VERSION) {
      throw "Local source app/VERSION is '$localVer', expected $LOCAL_TREE_VERSION. Refusing to build public main instead."
    }
    Say "    local source: $LocalTree"
    Say "    app/VERSION: $localVer"
    Say '    not cloning GitHub and not checking out public main'
    Step 'repo' (T 't_repo' "local $localVer") {
      $dest = [IO.Path]::GetFullPath($REPO)
      $src = [IO.Path]::GetFullPath($LocalTree)
      if ($dest -ne $src) {
        if ($src.StartsWith($dest + '\', [StringComparison]::OrdinalIgnoreCase)) {
          throw 'Install folder is inside the local source. Choose a different install folder.'
        }
        # Drop compile stamps so the build runs again. The build itself is
        # incremental: only what the sync below changed is compiled.
        foreach ($n in 'build-app', 'build-gpu', 'build') {
          Remove-Item -LiteralPath (Join-Path $STAMPS "$n.done") -Force -ErrorAction SilentlyContinue
        }
        Remove-Item -LiteralPath (Join-Path $STAMPS 'build.rev') -Force -ErrorAction SilentlyContinue
        if ($Update) { Test-AfroRunning }
        # Sync, not replace. Before 0.2.1 the whole folder was deleted and copied
        # again, which also deleted app\out: every update recompiled the game
        # from scratch, and every copied file looked newer than its object.
        # Now a file is written only when its bytes differ, and a written file
        # gets the current time, so Ninja rebuilds exactly what changed.
        # Kept as they are: the build folder, codegen output and game-derived
        # files. None of them are in the source tree.
        # app\region: Europe/USA codegen inputs written by tools\region_config.py.
        # Removing them would give them new timestamps and rerun codegen on every update.
        $sync = Sync-SourceTree $src $dest @('app\out', 'app\generated', 'app\assets', 'app\afro_switch_tables.toml', 'app\afro_icon.ico', 'app\region')
        Say "    source sync: $($sync.Changed) changed, $($sync.Same) unchanged, $($sync.Removed) removed"
        foreach ($rel in $sync.ChangedFiles) { Write-InstallLog "      changed: $rel" }
        foreach ($rel in $sync.RemovedFiles) { Write-InstallLog "      removed: $rel" }
      }
    }
    $REV = "local-$localVer"
    if (Test-Path -LiteralPath (Join-Path $REPO '.git')) {
      try {
        $head = "$(Git @('-C', $REPO, 'rev-parse', 'HEAD') | Select-Object -Last 1)".Trim()
        if ($head) { $REV = "local-$localVer-$head" }
      } catch { }
    }
  } else {
    if ($NonInteractive -and -not $PSBoundParameters.ContainsKey('Ref')) {
      throw 'Refusing to build the default branch main. Pass -LocalTree with the 0.3.4 source, or an explicit -Ref that is not public main.'
    }
    if ($NonInteractive -and $Ref -eq 'main') {
      throw 'Refusing -Ref main. Public main is not this 0.3.4 source.'
    }
    Step 'repo' (T 't_repo' $Ref) {
      Add-OwnedGit $REPO
      if (-not (Test-Path -LiteralPath (Join-Path $REPO '.git'))) {
        Remove-IncompleteClone $REPO 'afrorecomp'
        Git @('clone', $RepoUrl, $REPO)
      }
      if ($REPO -ne $here2) {
        Git @('-C', $REPO, 'fetch', '--tags', $RepoUrl, $Ref)
        Git @('-C', $REPO, 'checkout', '--detach', 'FETCH_HEAD')
      }
    }
    $REV = "$(Git @('-C', $REPO, 'rev-parse', 'HEAD') | Select-Object -Last 1)".Trim()
  }
  # Build stamps are only valid for the source revision they were built from:
  # a new commit makes build-app / build-gpu run again (incremental build).
  $revFile = Join-Path $STAMPS 'build.rev'
  $oldRev = if (Test-Path -LiteralPath $revFile) { (Get-Content -LiteralPath $revFile -Raw).Trim() } else { '' }
  if ($oldRev -ne $REV) {
    foreach ($n in 'build-app', 'build-gpu', 'build') { Remove-Item -LiteralPath (Join-Path $STAMPS "$n.done") -Force -ErrorAction SilentlyContinue }
    Say "    source revision: $REV (previous build: $(if ($oldRev) { $oldRev } else { 'unknown' })) -> rebuild"
  }
  Step 'sdk' 'ReXGlue SDK nightly 0.10.0.24-dev.gbd833a2' {
    Extract (Fetch 'rexsdk') (Join-Path $Root 'sdk')
    if (-not (Test-Path -LiteralPath (Join-Path $Root 'sdk\win-amd64\bin\rexglue.exe'))) {
      # zip layouts differ between nightlies: find rexglue.exe and move its parent into sdk\win-amd64
      $exe = Get-ChildItem -LiteralPath (Join-Path $Root 'sdk') -Recurse -Filter rexglue.exe | Select-Object -First 1
      if (-not $exe) { throw (T 'missing' 'rexglue.exe' 'SDK zip') }
      $top = $exe.Directory.Parent.FullName; $tmp = Join-Path $Root 'sdk.tmp'
      Move-Item -LiteralPath $top $tmp; Remove-Item -LiteralPath (Join-Path $Root 'sdk') -Recurse -Force
      New-Item -ItemType Directory -Force -Path (Join-Path $Root 'sdk') | Out-Null
      Move-Item -LiteralPath $tmp (Join-Path $Root 'sdk\win-amd64')
    }
  }
  Step 'rexglue-src' (T 't_rexsrc' $REXGLUE_COMMIT) {
    $s = Join-Path $Root 'rexglue-sdk'
    Add-OwnedGit $s
    if (-not (Test-Path -LiteralPath (Join-Path $s '.git'))) {
      # A previous attempt may have left the directory before .git existed.
      # If .git exists, this is skipped and the checkout below resumes it.
      Remove-IncompleteClone $s 'rexglue-sdk'
      # blobless clone: full history (bd833a2 is reachable) but file contents only for the checkout
      Git @('-c', 'core.longpaths=true', 'clone', '--filter=blob:none', '--no-checkout', $REXGLUE_GIT, $s)
    }
    Git @('-C', $s, '-c', 'advice.detachedHead=false', '-c', 'core.longpaths=true', 'checkout', '-f', $REXGLUE_COMMIT)
    # submodules: shallow at the recorded commits (much smaller), full fetch as a fallback
    try { Git @('-C', $s, '-c', 'core.longpaths=true', 'submodule', 'update', '--init', '--recursive', '--depth', '1', '--jobs', '8') }
    catch { Say (T 'shallow') Yellow; Git @('-C', $s, '-c', 'core.longpaths=true', 'submodule', 'update', '--init', '--recursive', '--jobs', '8') }
  }

  # ------------------------------------------------------- game data -------
  Step 'gamedata' (T 't_gamedata') {
    $g = Join-Path $Root 'gamedata'
    if ($GameDataDir) {
      Say (T 'copying' $GameDataDir)
      $srcGame = [IO.Path]::GetFullPath($GameDataDir)
      $files = @(Get-ChildItem -LiteralPath $srcGame -Recurse -File -Force -ErrorAction SilentlyContinue)
      $totalBytes = [long]0
      foreach ($file in $files) { $totalBytes += [long]$file.Length }
      $copied = [long]0
      Emit-Step 'gamedata' -1 0 $totalBytes 'copybytes'
      New-Item -ItemType Directory -Force -Path $g | Out-Null
      foreach ($file in $files) {
        $rel = $file.FullName.Substring($srcGame.Length).TrimStart('\')
        $target = Join-Path $g $rel
        $parent = Split-Path $target -Parent
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
        $copied += [long]$file.Length
        Emit-Step 'gamedata' -1 $copied $totalBytes 'copybytes'
      }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $g 'default.xex'))) {
      Run (Join-Path $TOOLS 'python\python.exe') @((Join-Path $REPO 'tools\xiso_extract.py'), $Iso, $g, '/')
    }
    if (-not (Test-Path -LiteralPath (Join-Path $g 'default.xex'))) { throw (T 'missing1' 'gamedata\default.xex') }
  }
  # Europe or USA: the build picks the matching hooks (tools/region_config.py).
  $xexSha = (Get-FileHash -LiteralPath (Join-Path $Root 'gamedata\default.xex') -Algorithm SHA1).Hash.ToLowerInvariant()
  $xexRegion = @{ '5e831c64173b4a24805bc9c0c0eb860b3d7f53c6' = 'Europe'; '873fff7191be2b687eb55f8564f9b9f5c5842b40' = 'USA' }[$xexSha]
  if (-not $xexRegion) { throw (T 'xex_bad' $xexSha) }
  Say (T 'xex_ok' $xexRegion)

  # ------------------------------------------------------- build -----------
  # app\out is kept from the previous build, so Ninja compiles only what
  # changed. A different compiler or Windows SDK starts it from scratch.
  $appOut = Join-Path $REPO 'app\out'
  $toolKeyFile = Join-Path $STAMPS 'build-tools.key'
  $toolKey = Get-BuildToolKey
  $oldToolKey = if (Test-Path -LiteralPath $toolKeyFile) { (Get-Content -LiteralPath $toolKeyFile -Raw).Trim() } else { '' }
  if (Test-Path -LiteralPath $appOut) {
    if ($oldToolKey -and $oldToolKey -ne $toolKey -and $REPO -ne $here2) {
      Say '    toolchain changed: app build folder cleared, full compile'
      Remove-Item -LiteralPath $appOut -Recurse -Force
    } else {
      Say '    app build folder kept: only changed sources are compiled'
    }
  }
  Set-Content -LiteralPath $toolKeyFile -Value $toolKey -Encoding ASCII
  Step 'build-app' (T 't_app') {
    $script:BASE = 0.25; $script:NinjaSteps = ''; Emit-Step 'codegen' -1 0 0 'generate'
    Run "$env:SystemRoot\System32\cmd.exe" @('/c', 'tools\build_win.bat') $REPO
    Say "    game build steps run: $(if ($script:NinjaSteps) { $script:NinjaSteps } else { 'none, already up to date' })"
  }
  Step 'build-gpu' (T 't_gpu') {
    $script:BASE = 0.1; $script:NinjaSteps = ''; $script:PhaseKey = 'gpu'; Emit-Step 'gpu' -1
    Run "$env:SystemRoot\System32\cmd.exe" @('/c', 'tools\build_gpu_plugin.bat') $REPO
    Say "    GPU plugin build steps run: $(if ($script:NinjaSteps) { $script:NinjaSteps } else { 'none, already up to date' })"
  }

  $script:STEP = 'verify output'; $script:CUR = 'verify'; Prog 0 -Force
  Emit-Step 'install' -1 0 0 'check'
  $out = Join-Path $REPO 'app\out\build\win-amd64-release'
  foreach ($c in @(@('afrorecomp.exe', $null), @('rexruntime.dll', 'afro_xma_frameless_chain'), @('rexgpu-xenos.dll', 'afro_edram_no_wrap'))) {
    $f = Join-Path $out $c[0]
    if (-not (Test-Path -LiteralPath $f)) { throw (T 'missing' $c[0] $out) }
    if ($c[1]) {
      $txt = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($f))
      if (-not $txt.Contains($c[1])) { throw (T 'notpatched' $c[0] $c[1]) }
    }
    Say (T 'ok_file' $c[0])
  }
  Mark 'build'; Set-Content -LiteralPath $revFile -Value $REV -Encoding ASCII; Finish 'verify'

  # ------------------------------------------------------- game folder -----
  # Self-contained copy of what is needed to play: <root>\game (exe, DLLs, launcher) + Play.bat.
  # Shortcuts point here, so it keeps working if the build files are deleted later.
  $script:STEP = 'game folder'
  $GAME = Join-Path $Root 'game'
  New-Item -ItemType Directory -Force -Path $GAME | Out-Null
  $built = @(Get-ChildItem -LiteralPath $out -File | Where-Object {
    $_.Name -notmatch '\.(stock|pdb|ilk|lib|exp|ninja|cmake|log)$' -and $_.Name -notmatch '^(CMakeCache\.txt|\.ninja_.*|compile_commands\.json)$'
  })
  if ($Update) {
    Test-AfroRunning
    # Stage the new program files, then swap. The previous exe stays until every
    # new file is in the staging folder. On a failed swap, moved files are put back.
    # This is only that swap. A finished update is not rolled back afterwards.
    $stage = Join-Path $GAME '.update-staging'
    $prev = Join-Path $GAME '.update-prev'
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    $copyI = 0
    foreach ($file in $built) {
      Copy-Item -LiteralPath $file.FullName (Join-Path $stage $file.Name) -Force
      $copyI++
      Emit-Step 'install' -1 $copyI $built.Count 'copy'
    }
    foreach ($n in 'afrorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll') {
      if (-not (Test-Path -LiteralPath (Join-Path $stage $n))) {
        Remove-Item -LiteralPath $stage -Recurse -Force
        throw "New build is missing $n. The installed game folder was not changed."
      }
    }
    if (Test-Path -LiteralPath $prev) { Remove-Item -LiteralPath $prev -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $prev | Out-Null
    $moved = @()
    try {
      foreach ($file in (Get-ChildItem -LiteralPath $stage -File)) {
        $dest = Join-Path $GAME $file.Name
        if (Test-Path -LiteralPath $dest) {
          Move-Item -LiteralPath $dest (Join-Path $prev $file.Name) -Force
          $moved += $file.Name
        }
        Move-Item -LiteralPath $file.FullName $dest -Force
      }
    } catch {
      foreach ($name in $moved) {
        $back = Join-Path $prev $name
        if (Test-Path -LiteralPath $back) { Move-Item -LiteralPath $back (Join-Path $GAME $name) -Force }
      }
      throw "Replacing program files failed. Previous program files were put back. $($_.Exception.Message)"
    }
    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $prev -Recurse -Force -ErrorAction SilentlyContinue
    Say '    program files replaced after the build succeeded; saves and launcher settings were not changed'
  } else {
    $copyI = 0
    foreach ($file in $built) {
      Copy-Item -LiteralPath $file.FullName (Join-Path $GAME $file.Name) -Force
      $copyI++
      Emit-Step 'install' -1 $copyI $built.Count 'copy'
    }
  }
  $ico = Join-Path $REPO 'app\afro_icon.ico'
  if ((Test-Path -LiteralPath $ico) -and (-not $Update -or -not (Test-Path -LiteralPath (Join-Path $GAME 'afro_icon.ico')))) {
    Copy-Item -LiteralPath $ico (Join-Path $GAME 'afro_icon.ico') -Force
  }
  # Button prompt icons for non-Xbox controllers and the keyboard (0.1.2). The
  # GPU plugin reads them from <exe>\glyphs\<family>\. Refreshed on every run.
  $glyphSrc = Join-Path $REPO 'tools\artwork\glyphs'
  if (Test-Path -LiteralPath $glyphSrc) {
    foreach ($fam in (Get-ChildItem -LiteralPath $glyphSrc -Directory)) {
      $dst = Join-Path (Join-Path $GAME 'glyphs') $fam.Name
      New-Item -ItemType Directory -Force -Path $dst | Out-Null
      Get-ChildItem -LiteralPath $fam.FullName -File -Filter '*.png' | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName (Join-Path $dst $_.Name) -Force
      }
    }
    Say '    button icons copied to game\glyphs'
  }
  # RecompLauncher (recomplauncher.com): its manifest in the installation root
  # (where RecompLauncher installs the setup) and next to afrorecomp.exe (a game
  # folder added there as a build). It handles both layouts (0.2.1).
  $rlSrc = Join-Path $REPO 'tools\recomplauncher.json'
  if (Test-Path -LiteralPath $rlSrc) {
    Copy-Item -LiteralPath $rlSrc (Join-Path $GAME 'recomplauncher.json') -Force
    Copy-Item -LiteralPath $rlSrc (Join-Path $Root 'recomplauncher.json') -Force
  }
  $playBat = Join-Path $GAME 'Play.bat'
  # The old stock bat runs the exe in the foreground, so cmd stays open for the
  # whole session. start returns at once; the window stays only if start fails.
  # A bat the user edited is left alone. afro_language.cfg is not part of this.
  $playOld = @(
    '@echo off',
    'rem Start Afro Samurai Recomp. Log: ..\afro.log. Hold Shift at start for the launcher.',
    'cd /d "%~dp0"',
    'afrorecomp.exe --game_data_root="%~dp0..\gamedata" --gpu_plugin=xenos --log_file="%~dp0..\afro.log" %*'
  )
  $playNew = @(
    '@echo off',
    'rem Start Afro Samurai Recomp. Log: ..\afro.log. Hold Shift at start for the launcher.',
    'rem The game window is separate. This console closes unless start fails.',
    'cd /d "%~dp0"',
    'start "" "%~dp0afrorecomp.exe" --game_data_root="%~dp0..\gamedata" --gpu_plugin=xenos --log_file="%~dp0..\afro.log" %*',
    'if errorlevel 1 (',
    '  echo The game did not start. El juego no se ha iniciado. See ..\afro.log',
    '  if not defined AFRO_PLAY_NOPAUSE pause',
    '  exit /b 1',
    ')'
  )
  $playLines = @()
  if (Test-Path -LiteralPath $playBat) {
    $playLines = @((Get-Content -LiteralPath $playBat | ForEach-Object { $_.TrimEnd() }) | Where-Object { $_ -ne '' })
  }
  $sameAs = {
    param($want)
    if ($playLines.Count -ne $want.Count) { return $false }
    for ($i = 0; $i -lt $want.Count; $i++) { if ($playLines[$i] -ne $want[$i]) { return $false } }
    return $true
  }
  if (-not (Test-Path -LiteralPath $playBat) -or (& $sameAs $playOld) -or (& $sameAs $playNew)) {
    $playNew | Set-Content -LiteralPath $playBat -Encoding ASCII
    Say '    Play.bat starts the game and closes its own window'
  } else {
    Say '    Play.bat left as the user wrote it'
  }
  foreach ($n in 'afrorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll') { if (-not (Test-Path -LiteralPath (Join-Path $GAME $n))) { throw (T 'missing' $n $GAME) } }
  Say (T 'game_folder' $GAME)
  # A new install records the chosen language. An update leaves afro_language.cfg alone.
  if (-not $Update) {
    $cfg = Join-Path $GAME 'afro_language.cfg'
    $lines = @(if (Test-Path -LiteralPath $cfg) { Get-Content -LiteralPath $cfg | Where-Object { $_ -notmatch '^language=' } } else { 'ask=1' })
    @("language=$($LANG_XID[$script:LI])") + $lines | Set-Content -LiteralPath $cfg -Encoding ASCII
    Say (T 'lang_cfg' $LANG_NAMES[$script:LI])
  } else {
    Say '    launcher settings kept (afro_language.cfg not rewritten)'
  }

  # ------------------------------------------------------- shortcut --------
  $script:STEP = 'shortcuts'
  if ($Shortcut -eq 'yes') {
    $ws = New-Object -ComObject WScript.Shell
    foreach ($dir in @([Environment]::GetFolderPath('Desktop'), (Join-Path ([Environment]::GetFolderPath('Programs')) ''))) {
      $l = $ws.CreateShortcut((Join-Path $dir 'Afro Samurai Recomp.lnk'))
      $l.TargetPath = Join-Path $GAME 'Play.bat'; $l.WorkingDirectory = $GAME
      if (Test-Path -LiteralPath (Join-Path $GAME 'afro_icon.ico')) { $l.IconLocation = Join-Path $GAME 'afro_icon.ico' }
      $l.Save(); Say (T 'shortcut' $l.FullName)
    }
  }

  $mins = ((Get-Date) - $t0).TotalMinutes
  Write-Progress -Activity (T 'title') -Completed
  try { $Host.UI.RawUI.WindowTitle = 'AfroSamuraiRecomp installer [100%] done' } catch { }
  Say (T 'all_done') Green
  Say ''
  Say (T 'inst_ok' $mins) Green
  Say (T 'play' "$GAME\Play.bat")
  Say (T 'saves' "$([Environment]::GetFolderPath('MyDocuments'))\afrorecomp")
  Say (T 'logs' "$Root\afro.log" $LOG)

  # ------------------------------------------------------- optional cleanup ---
  # Only folders INSIDE the install root; never the ISO, gamedata, game, logs or saves.
  $script:STEP = 'cleanup'
  if (Test-VolumeOrShareRoot $Root) {
    throw 'Refusing to clean a drive or share root. Nothing was deleted.'
  }
  if ($NonInteractive -and -not $KeepBuildFiles -and -not $CleanupAfterInstall) {
    throw 'NonInteractive requires -KeepBuildFiles or -CleanupAfterInstall.'
  }
  $junk = @('tools', 'downloads', 'rexglue-sdk', 'sdk', 'afrorecomp', '.install') | ForEach-Object { Join-Path $Root $_ } |
    Where-Object { (Test-Path -LiteralPath $_) -and ([IO.Path]::GetFullPath($_)).StartsWith($Root + '\', [StringComparison]::OrdinalIgnoreCase) }
  if ($inRepo) { $junk = @($junk | Where-Object { $_ -ne $here2 }) }   # never delete the checkout the installer runs from
  $isoFull = if ($Iso) { [IO.Path]::GetFullPath($Iso) } else { '' }
  if ($isoFull) { $junk = @($junk | Where-Object { -not $isoFull.StartsWith($_ + '\', [StringComparison]::OrdinalIgnoreCase) }) }
  $bytes = 0; foreach ($j in $junk) { $bytes += (Get-ChildItem -LiteralPath $j -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum }
  $gb = $bytes / 1GB
  if ($junk.Count -gt 0 -and -not $KeepBuildFiles) {
    $doIt = [bool]$CleanupAfterInstall
    if (-not $CleanupAfterInstall) {
      $msg = T 'cl_msg' $gb $GAME
      try {
        Add-Type -AssemblyName System.Windows.Forms
        $r = [System.Windows.Forms.MessageBox]::Show($msg, (T 'cl_title'), 'YesNo', 'Question', 'Button2')
        $doIt = ($r -eq 'Yes')
      } catch { Write-Host $msg; $doIt = YesNo (T 'cl_q') 'n' }
    }
    if ($doIt) {
      Say (T 'cl_del' $gb)
      foreach ($j in $junk) { Remove-Item -LiteralPath $j -Recurse -Force -ErrorAction Continue; Say (T 'deleted' $j) }
      Say (T 'cl_freed' $gb) Green
    } else {
      Say (T 'cl_kept' $gb)
    }
  }
  exit 0
} catch {
  Say ''
  Write-Progress -Activity (T 'title') -Completed
  try { $Host.UI.RawUI.WindowTitle = "AfroSamuraiRecomp installer FAILED at: $script:STEP" } catch { }
  Say (T 'failed' $script:STEP) Red
  Say (T 'error' $_.Exception.Message) Red
  if ($script:LogFallback) { Say "Fallback log: $($script:LogFallback)" Yellow }
  Say (T 'full_log' $LOG) Red
  Say (T 'retry') Yellow
  exit 1
}
