<#
AfroSamuraiRecomp one-click installer (Windows 10/11 x64).

Asks for an install folder and your Afro Samurai (Europe) ISO, then downloads a
portable toolchain from official sources into <root>\tools (no admin rights, no
PATH/registry changes, no Visual Studio installer), gets the ReXGlue SDK, clones
this repository, extracts your disc and builds afrorecomp.exe.

Safe to re-run: finished steps are remembered in <root>\.install and skipped.
Nothing is ever uploaded. Log: <root>\install.log

Usage (normally just double-click install.bat):
  powershell -ExecutionPolicy Bypass -File install.ps1 [options]
Options (all optional; anything missing is asked interactively):
  -InstallDir <dir>      install folder (alias -Root); skips the folder picker
  -Iso <file>            your Afro Samurai (Europe) ISO
  -AcceptMsvcLicense     accept the Microsoft Visual Studio Build Tools license
                         without the interactive prompt
  -Shortcut yes|no       create desktop + Start menu shortcuts
  -Ref <branch|tag>      repository revision to build (default: main)
  -RepoUrl <url|path>    repository to clone (default: the GitHub repo)
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
  [switch]$CleanupAfterInstall, [switch]$KeepBuildFiles,
  [ValidateSet('', 'en', 'es', 'fr', 'it')][string]$Language = ''
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'Continue'
$script:DLSHARE = 0.9   # share of a download+extract step spent downloading
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$ISO_SHA1 = '29394DB1854DFC59C767D768C8D35E0A08ACB73D'
$NEED_GB = 20
# Pinned downloads (official sources) with SHA-256.
$DL = @{
  mingit = @{ url = 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.4/MinGit-2.56.0.4-64-bit.zip'; sha = 'e3bf78351fe777fba47ddee5950082f8a98bb0f02604fdc7ba3ee2cb68ffa877'; file = 'MinGit-2.56.0.4-64-bit.zip' }
  cmake  = @{ url = 'https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-windows-x86_64.zip'; sha = '326c826486d46a23cf9d5a1c9ee3a567d427bdd02c4a34071937a2a675cff004'; file = 'cmake-4.4.5-windows-x86_64.zip' }
  ninja  = @{ url = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'; sha = '07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65'; file = 'ninja-win-1.13.2.zip' }
  llvm   = @{ url = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/clang%2Bllvm-20.1.8-x86_64-pc-windows-msvc.tar.xz'; sha = 'f229769f11d6a6edc8ada599c0cda964b7dee6ab1a08c6cf9dd7f513e85b107f'; file = 'clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz' }
  python = @{ url = 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip'; sha = '4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3'; file = 'python-3.12.10-embed-amd64.zip' }
  pycryptodome = @{ url = 'https://files.pythonhosted.org/packages/e1/52/7d0830ccb961c9e7224b88a60df1a4eab7fa6c5fd426d7b7c972988a4cac/pycryptodome-4.3.0-cp39-abi3-win_amd64.whl'; sha = '195c9d781905605b3b9de8567c47986a9d0ee513566b5150eb9400692465b610'; file = 'pycryptodome-4.3.0-cp39-abi3-win_amd64.zip' }
  # portable-msvc.py by Martins Mozeiko (public domain): downloads MSVC + Windows SDK
  # directly from Microsoft's official Visual Studio manifests.
  pmsvc  = @{ url = 'https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/raw/49b58850aa5e5c1e65f090d4651ee76607883e11/portable-msvc.py'; sha = '1791cccca594ff083ce8d3e0a139e6156d2c312e24332a909e608010f0230a51'; file = 'portable-msvc.py' }
  rexsdk = @{ url = 'https://github.com/rexglue/rexglue-sdk/releases/download/nightly-20261002-bd833a2a/rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip'; sha = '1e19d7ec9be0d9d7e1a0088a34a33b068d4dea520181e81318528ef3b37627e7'; file = 'rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip' }
}
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
  'sel_iso' = @('Select your Afro Samurai (Europe) ISO...', 'Selecciona tu ISO de Afro Samurai (Europa)...', 'Selectionne ton ISO d''Afro Samurai (Europe)...', 'Seleziona la tua ISO di Afro Samurai (Europa)...')
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
  'iso_only' = @('Only the Europe disc is supported; another version or a bad dump will NOT work.', 'Solo se admite el disco europeo; otra version o un volcado malo NO funcionara.', 'Seul le disque europeen est pris en charge ; une autre version ou un mauvais dump ne marchera PAS.', 'E supportato solo il disco europeo; un''altra versione o un dump difettoso NON funzionera.')
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
  's_go' = @('  Starting now; no more questions. This takes about 30-60 minutes.', '  Empezando; no hay mas preguntas. Tarda unos 30-60 minutos.', '  C''est parti ; plus de questions. Cela prend environ 30-60 minutes.', '  Si parte; nessuna altra domanda. Ci vogliono circa 30-60 minuti.')
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
  't_app' = @('Build afrorecomp.exe (code generation + compile, 10-40 min)', 'Compilar afrorecomp.exe (generacion de codigo + compilacion, 10-40 min)', 'Compiler afrorecomp.exe (generation de code + compilation, 10-40 min)', 'Compilare afrorecomp.exe (generazione codice + compilazione, 10-40 min)')
  't_gpu' = @('Build patched GPU plugin and runtime', 'Compilar el plugin de GPU y el runtime parcheados', 'Compiler le plugin GPU et le runtime patches', 'Compilare plugin GPU e runtime con patch')
}
$script:LI = 0
function T($k) { $v = $TR[$k]; if (-not $v) { return $k }; $s = $v[$script:LI]; if ($args.Count) { $s = $s -f $args }; $s.Replace('`n', "`n") }

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c; if ($script:LOG) { "$(Get-Date -f 'yyyy-MM-dd HH:mm:ss') $m" | Add-Content -LiteralPath $script:LOG } }
function Ask($q, $def) { $a = Read-Host "$q [$def]"; if ([string]::IsNullOrWhiteSpace($a)) { $def } else { $a.Trim().Trim('"') } }
function YesNo($q, $def = 'y') {
  $yk = T 'yeskey'; if ($def -eq 'y') { $def = $yk }
  $a = (Ask "$q ($(T 'yn'))" $def).ToLower(); $a.StartsWith('y') -or $a.StartsWith($yk)
}
$script:STEP = 'start'; $script:BASE = 0
# Overall progress: each step weighted by its typical duration (minutes).
$PLAN = [ordered]@{ 'iso-hash' = 1; git = 1; cmake = 1; ninja = 0.3; python = 0.5; llvm = 8; msvc = 6; repo = 0.3; sdk = 1; 'rexglue-src' = 4; gamedata = 4; 'build-app' = 30; 'build-gpu' = 15; verify = 0.2 }
$TOTALW = ($PLAN.Values | Measure-Object -Sum).Sum
$script:DONEW = 0; $script:CUR = $null; $script:LASTLOGPCT = -10; $script:LASTDRAW = [DateTime]::MinValue
function Prog([double]$frac, [string]$detail = '', [switch]$Force) {
  $w = if ($script:CUR) { $PLAN[$script:CUR] } else { 0 }
  $pct = [math]::Min(99, [math]::Floor(100 * ($script:DONEW + $w * [math]::Max([double]0, [math]::Min([double]1, $frac))) / $TOTALW))
  $i = if ($script:CUR) { @($PLAN.Keys).IndexOf($script:CUR) + 1 } else { 0 }
  $line = "[{0,3}%] {4} {1}/{2}: {3}" -f $pct, $i, $PLAN.Count, $script:STEP, (T 'step')
  if ($detail) { $line += " ($detail)" }
  if (-not $Force -and ((Get-Date) - $script:LASTDRAW).TotalMilliseconds -lt 500) { return }
  $script:LASTDRAW = Get-Date
  try { $Host.UI.RawUI.WindowTitle = "AfroSamuraiRecomp installer $line" } catch { }
  Write-Progress -Activity (T 'title') -Status $line -PercentComplete $pct
  if ($Force -or $pct -ge $script:LASTLOGPCT + 5) { $script:LASTLOGPCT = $pct; Say $line White }
}
function Finish($n) { $script:DONEW += $PLAN[$n]; $script:CUR = $null }
function Done($n) { Test-Path -LiteralPath (Join-Path $STAMPS "$n.done") }
function Mark($n) { New-Item -ItemType File -Force -Path (Join-Path $STAMPS "$n.done") | Out-Null }
function Step($n, $title, [scriptblock]$body) {
  $script:STEP = $title; $script:CUR = $n
  if (Done $n) { Say (T 'skip' $title) DarkGray; Finish $n; return }
  Prog 0 -Force
  $t = Get-Date; & $body; Mark $n; Finish $n
  Say (T 'done_in' ((Get-Date) - $t).TotalSeconds) Green
}
function Fetch($key) {
  $d = $DL[$key]; $out = Join-Path $DLDIR $d.file
  if ((Test-Path -LiteralPath $out) -and ((Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash -eq $d.sha)) { return $out }
  Say (T 'dl' $d.url)
  $req = [Net.HttpWebRequest]::Create($d.url); $req.UserAgent = 'AfroSamuraiRecomp-installer'
  $resp = $req.GetResponse(); $len = $resp.ContentLength; $in = $resp.GetResponseStream()
  $fs = [IO.File]::Create("$out.part"); $buf = New-Object byte[] 1048576; $got = 0
  try {
    while (($r = $in.Read($buf, 0, $buf.Length)) -gt 0) {
      $fs.Write($buf, 0, $r); $got += $r
      if ($len -gt 0) { Prog ($got / $len * $script:DLSHARE) (T 'dl_prog' $d.file ($got / 1MB) ($len / 1MB)) }
    }
  } finally { $fs.Close(); $in.Close(); $resp.Close() }
  $h = (Get-FileHash -LiteralPath "$out.part" -Algorithm SHA256).Hash
  if ($h -ne $d.sha) { Remove-Item -LiteralPath "$out.part" -Force; throw (T 'sha_bad' $d.file $h $d.sha) }
  Move-Item -LiteralPath "$out.part" $out -Force
  Say (T 'sha_ok' ((Get-Item -LiteralPath $out).Length / 1MB))
  return $out
}
$TAR = Join-Path $env:SystemRoot 'System32\tar.exe'
function Extract($arc, $dest, [int]$strip = 0, [switch]$Into) {
  # Extracts with Windows' built-in tar (bsdtar: zip, tar.xz, ...), much faster than Expand-Archive.
  # Into a temporary folder that is renamed at the end, so a cancelled run never leaves a
  # half-extracted folder behind (-Into: add files to an existing folder instead).
  Prog $script:DLSHARE (T 'extracting' (Split-Path $arc -Leaf)) -Force
  $tmp = if ($Into) { $dest } else { "$dest.partial" }
  if (-not $Into -and (Test-Path -LiteralPath $tmp)) { Remove-Item -LiteralPath $tmp -Recurse -Force }
  New-Item -ItemType Directory -Force -Path $tmp | Out-Null
  $ok = $false
  if (Test-Path -LiteralPath $TAR) {
    $a = @('-xf', $arc, '-C', $tmp); if ($strip) { $a += @('--strip-components', "$strip") }
    & $TAR @a 2>&1 | ForEach-Object { Add-Content -LiteralPath $LOG "$_" }
    $ok = ($LASTEXITCODE -eq 0)
    if (-not $ok) { Say (T 'tar_fail' $LASTEXITCODE) Yellow }
  }
  if (-not $ok) {
    if ($strip -or $arc -notmatch '\.(zip|whl)$') { throw (T 'cant_extract' $arc) }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [IO.Compression.ZipFile]::OpenRead($arc)
    try { foreach ($e in $z.Entries) { $t = Join-Path $tmp $e.FullName; if ($e.Name -eq '') { New-Item -ItemType Directory -Force -Path $t | Out-Null; continue }; New-Item -ItemType Directory -Force -Path (Split-Path $t -Parent) | Out-Null; [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $t, $true) } } finally { $z.Dispose() }
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
  try {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    & $exe @a 2>&1 | ForEach-Object {
      $s = "$_"; Add-Content -LiteralPath $LOG $s
      if ($s -match '^\[(\d+)/(\d+)\]') { Prog ($script:BASE + (1.0 - $script:BASE) * [double]$Matches[1] / [double]$Matches[2]) (T 'compiling' $Matches[1] $Matches[2]) }
      elseif ($s -match 'codegen|CODEGEN') { Write-Host "    $s" }
      elseif ($s -notmatch '^\[') { Write-Host "    $s" }
    }
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    if ($code -ne 0) { throw (T 'exit_code' $exe $code) }
  } finally { if ($wd) { Pop-Location } }
}
function Git([string[]]$a, $wd = $null) { Run (Join-Path $TOOLS 'git\cmd\git.exe') $a $wd }

# ---------------------------------------------------------------- setup ------
# Language first (before anything else). Default: Windows UI language if supported.
$ui = try { (Get-UICulture).TwoLetterISOLanguageName } catch { 'en' }
$script:LI = [math]::Max(0, $LANGS.IndexOf($ui))
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
$Root = $InstallDir
while (-not $Root) {
  $start = if ($inRepo) { Split-Path (Split-Path $here -Parent) -Parent } else { $here }
  Write-Host (T 'choose_folder')
  $parent = PickFolder $start
  if (-not $parent) { $parent = Ask (T 'no_folder') $start }
  $cand = [IO.Path]::GetFullPath((Join-Path $parent $NAME))
  if ($inRepo -and $cand -eq (Split-Path $here -Parent)) { $cand = "$cand-install" }
  Write-Host ''; Write-Host (T 'install_folder' $cand) -ForegroundColor Cyan
  CheckPath $cand
  if (YesNo (T 'install_there') 'y') { $Root = $cand }
}
$Root = [IO.Path]::GetFullPath($Root)
New-Item -ItemType Directory -Force -Path $Root | Out-Null
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

try {
  # ------------------------------------------------------- questions -------
  # Every question is asked here, before any download, so the rest runs unattended.
  Write-Host ''; Write-Host (T 'all_q') -ForegroundColor Cyan
  $script:STEP = 'choose game data'
  $haveGame = Test-Path -LiteralPath (Join-Path $Root 'gamedata\default.xex')
  if (-not $haveGame -and -not $GameDataDir -and -not $Iso) {
    Say (T 'sel_iso')
    try {
      Add-Type -AssemblyName System.Windows.Forms
      $dlg = New-Object System.Windows.Forms.OpenFileDialog
      $dlg.Title = 'Select your Afro Samurai (Europe) ISO'; $dlg.Filter = 'Disc images (*.iso)|*.iso|All files (*.*)|*.*'
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
    Say (T 'iso_check')
    $h = (Get-FileHash -LiteralPath $Iso -Algorithm SHA1).Hash
    if ($h -eq $ISO_SHA1) { Say (T 'iso_ok') Green; Mark 'iso-hash' }
    else {
      Say (T 'iso_bad' $h $ISO_SHA1) Yellow
      Say (T 'iso_only') Yellow
      if (-not (YesNo (T 'cont') 'n')) { throw (T 'iso_nomatch') }
    }
  }
  Finish 'iso-hash'

  # Microsoft license for the portable MSVC compiler + Windows SDK (only if not installed yet).
  $script:STEP = 'Microsoft license'
  $needMsvc = -not (Done 'msvc')
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
  if (-not $Shortcut) { $Shortcut = if (YesNo (T 'sc_q') 'y') { 'yes' } else { 'no' } }
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
    Run $py @('portable-msvc.py', '--accept-license') $TOOLS
    if (-not (Test-Path -LiteralPath (Join-Path $TOOLS 'msvc\setup_x64.bat'))) { throw (T 'missing1' 'msvc\setup_x64.bat') }
    Remove-Item -LiteralPath (Join-Path $TOOLS 'downloads') -Recurse -Force -ErrorAction SilentlyContinue
  }

  # ------------------------------------------------------- sources ---------
  # The source step always runs (no stamp), so a re-run picks up new commits.
  Remove-Item -LiteralPath (Join-Path $STAMPS 'repo.done') -Force -ErrorAction SilentlyContinue
  Step 'repo' (T 't_repo' $Ref) {
    if (-not (Test-Path -LiteralPath (Join-Path $REPO '.git'))) {
      if (Test-Path -LiteralPath $REPO) { Remove-Item -LiteralPath $REPO -Recurse -Force }
      Git @('clone', $RepoUrl, $REPO)
    }
    if ($REPO -ne $here2) {
      Git @('-C', $REPO, 'fetch', '--tags', $RepoUrl, $Ref)
      Git @('-C', $REPO, 'checkout', '--detach', 'FETCH_HEAD')
    }
  }
  # Build stamps are only valid for the source revision they were built from:
  # a new commit makes build-app / build-gpu run again (incremental build).
  $REV = (& (Join-Path $TOOLS 'git\cmd\git.exe') -C $REPO rev-parse HEAD).Trim()
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
    if (-not (Test-Path -LiteralPath (Join-Path $s '.git'))) {
      if (Test-Path -LiteralPath $s) { Remove-Item -LiteralPath $s -Recurse -Force }
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
      & "$env:SystemRoot\System32\robocopy.exe" $GameDataDir $g /E /NFL /NDL /NJH /NP | Out-Null
      if ($LASTEXITCODE -ge 8) { throw (T 'robocopy' $LASTEXITCODE) }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $g 'default.xex'))) {
      Run (Join-Path $TOOLS 'python\python.exe') @((Join-Path $REPO 'tools\xiso_extract.py'), $Iso, $g, '/')
    }
    if (-not (Test-Path -LiteralPath (Join-Path $g 'default.xex'))) { throw (T 'missing1' 'gamedata\default.xex') }
  }

  # ------------------------------------------------------- build -----------
  Step 'build-app' (T 't_app') { $script:BASE = 0.25; Prog 0.02 (T 'codegen') -Force; Run "$env:SystemRoot\System32\cmd.exe" @('/c', 'tools\build_win.bat') $REPO }
  Step 'build-gpu' (T 't_gpu') { $script:BASE = 0.1; Run "$env:SystemRoot\System32\cmd.exe" @('/c', 'tools\build_gpu_plugin.bat') $REPO }

  $script:STEP = 'verify output'; $script:CUR = 'verify'; Prog 0 -Force
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
  Get-ChildItem -LiteralPath $out -File | Where-Object {
    $_.Name -notmatch '\.(stock|pdb|ilk|lib|exp|ninja|cmake|log)$' -and $_.Name -notmatch '^(CMakeCache\.txt|\.ninja_.*|compile_commands\.json)$'
  } | ForEach-Object { Copy-Item -LiteralPath $_.FullName (Join-Path $GAME $_.Name) -Force }
  $ico = Join-Path $REPO 'app\afro_icon.ico'; if (Test-Path -LiteralPath $ico) { Copy-Item -LiteralPath $ico (Join-Path $GAME 'afro_icon.ico') -Force }
  # Same options as tools\run_win.bat.
  @('@echo off', 'rem Start Afro Samurai Recomp. Log: ..\afro.log. Hold Shift at start for the launcher.', 'cd /d "%~dp0"',
    'afrorecomp.exe --game_data_root="%~dp0..\gamedata" --gpu_plugin=xenos --readback_resolve=fast --log_file="%~dp0..\afro.log" %*') |
    Set-Content -LiteralPath (Join-Path $GAME 'Play.bat') -Encoding ASCII
  foreach ($n in 'afrorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll') { if (-not (Test-Path -LiteralPath (Join-Path $GAME $n))) { throw (T 'missing' $n $GAME) } }
  Say (T 'game_folder' $GAME)
  # Preset the launcher language (afro_language.cfg next to afrorecomp.exe, which is
  # where the launcher reads it). Only the language= line is set; ask= stays 1 (or
  # whatever the user saved), so the launcher still opens and Remember works as before.
  $cfg = Join-Path $GAME 'afro_language.cfg'
  $lines = @(if (Test-Path -LiteralPath $cfg) { Get-Content -LiteralPath $cfg | Where-Object { $_ -notmatch '^language=' } } else { 'ask=1' })
  @("language=$($LANG_XID[$script:LI])") + $lines | Set-Content -LiteralPath $cfg -Encoding ASCII
  Say (T 'lang_cfg' $LANG_NAMES[$script:LI])

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
  Say (T 'full_log' $LOG) Red
  Say (T 'retry') Yellow
  exit 1
}
