import json
import os
import subprocess
import sys

def main():
    # Carica le task generate nel passaggio precedente
    try:
        with open("tasks.json", "r") as f:
            tasks = json.load(f)
    except FileNotFoundError:
        print("Errore: Il file tasks.json non è stato trovato. Assicurati che il task precedente sia andato a buon fine.")
        sys.exit(1)
    except json.JSONDecodeError:
        print("Errore: Il file tasks.json non contiene un JSON valido.")
        sys.exit(1)

    print(f"Trovate {len(tasks)} task da eseguire. Inizializzazione di Jules...")

    for task in tasks:
        task_id = task.get("task_id", "Sconosciuto")
        description = task.get("description", "")
        target_files = ", ".join(task.get("target_files", []))

        # Prompt super-ottimizzato per Embedded C++ e Jules
        jules_prompt = f"""
        SEI UN SENIOR EMBEDDED C++ ENGINEER. Stai lavorando sul progetto 'ha-ecoflow-ble' (PlatformIO, ESP32).
        
        IL TUO TASK:
        {description}
        
        FILE COINVOLTI:
        {target_files}
        
        REGOLE FONDAMENTALI (PENALITÀ MASSIMA SE IGNORATE):
        1. AGGIORNAMENTO CONTESTO: Prima di scrivere o modificare codice, DEVI forzare l'aggiornamento dell'index RAG della repository per assicurarti di avere il contesto più recente.
        2. BEST PRACTICES EMBEDDED: 
           - Gestisci correttamente la memoria della MCU.
           - Capisci la struttura dell'MCU e sappi che hai le schematiche in Docs se richieste.
           - Non rimuovere funzionalità.
           - Commenta e documenta sempre ciò che crei, mantenedo aggiornata la documentazione. 
        3. AUTO-VALIDAZIONE E COMPILAZIONE: Prima di considerare il task completato e prima di preparare qualsiasi Push Request/Commit, DEVI eseguire il comando `pio run` nel tuo ambiente shell integrato.
        4. RISOLUZIONE ERRORI: Se `pio run` fallisce, non procedere. Analizza gli errori del compilatore, correggi il codice e ritenta la compilazione finché non passa con successo.
        """

        print(f"\n--- Esecuzione Task {task_id} con Jules ---")
        
        # ESEMPIO DI CHIAMATA CLI (sostituisci 'jules-cli' con il comando reale di Jules)
        # Passiamo il prompt come argomento. Potrebbe essere necessario adattarlo
        # a come Jules accetta input (es. tramite un file temporaneo se il prompt è troppo lungo).
        try:
            # Creiamo un file temporaneo per il prompt per evitare problemi di escaping nella shell
            prompt_file = f"prompt_task_{task_id}.txt"
            with open(prompt_file, "w") as pf:
                pf.write(jules_prompt)

            # Esecuzione ipotetica: jules --file prompt_task_1.txt
            comando_jules = ["jules", "run", "--prompt-file", prompt_file]
            
            # Eseguiamo il comando e stampiamo l'output in tempo reale nel log della Action
            process = subprocess.Popen(comando_jules, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            for line in process.stdout:
                print(line, end="")
            
            process.wait()
            
            if process.returncode != 0:
                print(f"Attenzione: Jules ha riportato un errore nel task {task_id}.")
                # Decidi se fermare tutto (sys.exit(1)) o continuare con il prossimo task
                
            # Pulizia file temporaneo
            os.remove(prompt_file)

        except FileNotFoundError:
            print("Errore: Eseguibile di Jules non trovato. Assicurati che sia installato nel PATH del tuo MacBook.")
            sys.exit(1)
        except Exception as e:
            print(f"Errore durante l'esecuzione di Jules: {e}")
            sys.exit(1)

    print("\nTutti i task assegnati a Jules sono stati completati.")

if __name__ == "__main__":
    main()
